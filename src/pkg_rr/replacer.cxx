#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>

#include <pkgxx/config.h>
#include <pkgxx/progress_bar.hxx>
#include <pkgxx/string_algo.hxx>

#include "pkg_chk/check.hxx"
#include "replacer.hxx"

using namespace std::chrono_literals;
using namespace pkgxx::tty::literals;
namespace fs = std::filesystem;
namespace tty = pkgxx::tty;

namespace {
    struct replace_failed: virtual std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct source_checker: virtual pkg_chk::source_checker_base {
        source_checker(pkg_rr::environment const& env)
            : checker_base(
                false, // add_missing (-a)
                env.opts.check_build_version,
                env.opts.concurrency,
                true,  // update (-u)
                false, // delete_mismatched (-r)
                env.PKG_INFO)
            , source_checker_base(env.PKGSRCDIR)
            , _env(env) {}

    protected:
        virtual void total(std::size_t num) const override {
            _pb = std::make_unique<pkgxx::progress_bar>(num);
        }

        virtual void progress() const override {
            (*_pb)++;
        }

        virtual void done() const override {
            _pb.reset();
        }

        virtual void
        msg(std::function<void (pkgxx::ttystream_base&)> const& f) const override {
            _pb->message([&](auto& out) {
                auto out_ = _env.msg(out);
                f(out_);
            });
        }

        virtual void
        warn(std::function<void (pkgxx::ttystream_base&)> const& f) const override {
            _pb->message([&](auto& out) {
                auto out_ = _env.warn(out);
                f(out_);
            });
        }

        virtual void
        verbose(std::function<void (pkgxx::ttystream_base&)> const&) const override {
            // Don't show verbose messages from pkg_chk.
        }

        virtual void
        fatal(std::function<void (pkgxx::ttystream_base&)> const& f) const override {
            _pb->message([&](auto& out) {
                _env.fatal(f, out);
            });
        }

        pkg_rr::environment _env;
        mutable std::unique_ptr<pkgxx::progress_bar> _pb;
    };

    std::optional<pkgxx::pkgbase>
    obvious_pkgbase_of(pkgxx::pkgpattern const& pat) {
        return std::visit(
            [&](auto const& p) -> std::optional<pkgxx::pkgbase> {
                if constexpr (std::is_same_v<pkgxx::pkgpattern::version_range const&, decltype(p)>) {
                    return p.base;
                }
                else {
                    return std::nullopt;
                }
            },
            static_cast<pkgxx::pkgpattern::pattern_type const&>(pat));
    }
}

namespace pkg_rr {
    rolling_replacer::rolling_replacer(
        std::filesystem::path const& progname_,
        options const& opts_,
        environment const& env_)
        : progname(progname_)
        , opts(opts_)
        , env(env_)
        , UNSAFE_VAR(opts.strict ? "unsafe_depends_strict" : "unsafe_depends")
        , pattern_to_base_cache(0)
        , _pkgname_sty(tty::bold)
        , _new_deps_sty(tty::faint)
        , _var_sty(tty::faint)
        , _even_sty(tty::dull_colour(tty::magenta))
        , _odd_sty(tty::dull_colour(tty::cyan)) {

        std::future<todo_type> MISMATCH_TODO_f;
        std::future<todo_type> REBUILD_TODO_f;
        std::future<todo_type> UNSAFE_TODO_f;
        {
            pkg_rr::package_scanner scanner(env.PKG_INFO.get(), opts.concurrency);
            MISMATCH_TODO_f = check_mismatch(scanner);
            REBUILD_TODO_f  = check_rebuild(scanner);
            UNSAFE_TODO_f   = check_unsafe(scanner);
        }
        MISMATCH_TODO = MISMATCH_TODO_f.get();
        REBUILD_TODO  = REBUILD_TODO_f.get();
        UNSAFE_TODO   = UNSAFE_TODO_f.get();
        refresh_todo();

        auto const& mark_as_installed =
            [&](todo_type const& todo) {
                for (auto const& [base, _path]: todo) {
                    definitely_installed.insert(base);
                }
            };
        mark_as_installed(MISMATCH_TODO);
        mark_as_installed(REBUILD_TODO);
        mark_as_installed(UNSAFE_TODO);

        topology = initial_topology = depgraph_installed();
        dump_todo();
    }

    void
    rolling_replacer::run() {
        while (!REPLACE_TODO.empty()) {
            auto [base, path] = choose_one();
            try {
                if (DEPENDS_CHECKED.count(base)) {
                    env.msg() << "Selecting " << _pkgname_sty(base) << " ("
                              << static_cast<std::filesystem::path const&>(path).string()
                              << ") as next package to replace" << std::endl;
                    env.vsleep(1s);
                }
                else {
                    auto const version = update_depends_with_source(base, path);
                    DEPENDS_CHECKED.emplace(base, version);
                    continue;
                }

                if (opts.just_fetch) {
                    fetch(base, path);
                }
                else {
                    replace(base, path);
                }
                SUCCEEDED.push_back(base);
            }
            catch (replace_failed const& e) {
                FAILED.push_back(base);

                if (opts.continue_on_errors) {
                    env.error() << e.what() << std::endl;
                }
                else {
                    abort([&](auto& out) {
                        out << e.what() << std::endl;
                    });
                }
            }

            // Remove just-replaced package from all *_TODO lists
            // regardless of whether it succeeded or not.
            MISMATCH_TODO.erase(base);
            REBUILD_TODO.erase(base);
            MISSING_TODO.erase(base);
            UNSAFE_TODO.erase(base);

            refresh_todo();
            dump_todo();
            env.vsleep(2s);
        }
        env.msg() << "No more packages to replace; done." << std::endl;
        report();
    }

    std::future<rolling_replacer::todo_type>
    rolling_replacer::check_mismatch(pkg_rr::package_scanner& scanner) const {
        if (opts.check_for_updates) {
            env.msg() << "Checking for mismatched installed packages by scanning source tree" << std::endl;
            auto result = source_checker(env).run();

            if (!result.MISMATCH_TODO.empty()) {
                // Spawn xargs only if it's non-empty; otherwise we would
                // end up asking for password for nothing.
                env.msg() << "Marking outdated packages as mismatched" << std::endl;

                pkgxx::harness xargs =
                    spawn_su(std::string(CFG_XARGS) + ' ' + env.PKG_ADMIN.get() + " set mismatch=YES");
                for (auto const& [name, _]: result.MISMATCH_TODO) {
                    xargs.cin() << name << std::endl;
                }
                xargs.cin().close();

                if (xargs.wait_exit().status != 0) {
                    env.warn() << "mismatch variable not set due to permissions; "
                               << "the status will not persist." << std::endl;
                }
            }

            // pkg_chk gives us PKGNAME but we want PKGBASE.
            todo_type todo;
            for (auto&& [name, path]: result.MISMATCH_TODO) {
                todo.emplace(std::move(name.base), std::move(path));
            }

            std::promise<todo_type> res;
            res.set_value(std::move(todo));
            return res.get_future();
        }
        else {
            env.msg() << "Checking for mismatched installed packages "
                      << _var_sty("(mismatch=YES)") << std::endl;
            return scanner.add_axis("mismatch", opts.no_check);
        }
    }

    std::future<rolling_replacer::todo_type>
    rolling_replacer::check_rebuild(pkg_rr::package_scanner& scanner) const {
        if (opts.just_fetch) {
            std::promise<todo_type> res;
            res.set_value({});
            return res.get_future();
        }
        else {
            env.msg() << "Checking for rebuild-requested installed packages "
                      << _var_sty("(rebuild=YES)") << std::endl;
            return scanner.add_axis("rebuild");
        }
    }

    std::future<rolling_replacer::todo_type>
    rolling_replacer::check_unsafe(pkg_rr::package_scanner& scanner) const {
        if (opts.just_fetch) {
            std::promise<todo_type> res;
            res.set_value({});
            return res.get_future();
        }
        else {
            env.msg() << "Checking for unsafe installed packages "
                      << _var_sty('('_ch << UNSAFE_VAR << "=YES)") << std::endl;
            return scanner.add_axis(UNSAFE_VAR);
        }
    }

    void
    rolling_replacer::recheck_unsafe(pkgxx::pkgbase const& base) {
        env.msg() << "Re-checking for unsafe installed packages "
                  << _var_sty('('_ch << UNSAFE_VAR << "=YES)") << std::endl;
        auto const& PKG_INFO = env.PKG_INFO.get();
        pkgxx::guarded<todo_type> unsafe_pkgs;
        {
            pkgxx::nursery n(opts.concurrency);
            for (auto const& unsafe_pkg: pkgxx::who_requires(PKG_INFO, base)) {
                if (UNSAFE_TODO.count(unsafe_pkg.base) > 0) {
                    // Already in the set. Skip it.
                    continue;
                }
                else if (opts.dry_run) {
                    // With -n, the replace didn't happen, and thus the
                    // packages that would have been marked
                    // unsafe_depends=YES were not. Add the set that would
                    // have been marked so we can watch what the actual run
                    // would have done.
                    //
                    // Note that this is only an approximation because
                    // "make replace" marks packages as unsafe only when it
                    // has potentially caused an ABI change. We don't want
                    // to replicate the logic just for our dry-run.
                    n.start_soon(
                        [&, unsafe_pkg = std::move(unsafe_pkg)]() {
                            auto build_info  = pkgxx::build_info(PKG_INFO, unsafe_pkg);
                            auto unsafe_path = build_info.find("PKGPATH");
                            assert(unsafe_path != build_info.end());
                            unsafe_pkgs.lock()->emplace(unsafe_pkg.base, unsafe_path->second);
                        });
                }
                else {
                    n.start_soon(
                        [&, unsafe_pkg = std::move(unsafe_pkg)]() {
                            std::optional<pkgxx::pkgpath> unsafe_path;
                            for (auto const& [var, value]: pkgxx::build_info(PKG_INFO, unsafe_pkg)) {
                                if (var == "PKGPATH") {
                                    unsafe_path.emplace(value);
                                }
                                else if (var == UNSAFE_VAR && pkgxx::ci_equal(value, "yes")) {
                                    assert(unsafe_path.has_value());
                                    unsafe_pkgs.lock()->emplace(unsafe_pkg.base, *unsafe_path);
                                }
                            }
                        });
                }
            }
        }
        for (auto const& unsafe_pkg: *(unsafe_pkgs.lock())) {
            auto const& [unsafe_base, _unsafe_path] = unsafe_pkg;
            topology.add_edge(unsafe_base, base);
            UNSAFE_TODO.insert(unsafe_pkg);
        }
    }

    void
    rolling_replacer::refresh_todo() {
        if (opts.just_fetch) {
            REPLACE_TODO = MISMATCH_TODO;
            REPLACE_TODO.insert(MISSING_TODO.begin(), MISSING_TODO.end());
        }
        else {
            REPLACE_TODO = MISMATCH_TODO;
            REPLACE_TODO.insert(REBUILD_TODO.begin(), REBUILD_TODO.end());
            REPLACE_TODO.insert(MISSING_TODO.begin(), MISSING_TODO.end());
            REPLACE_TODO.insert(UNSAFE_TODO.begin(), UNSAFE_TODO.end());
        }

        for (auto const& base: opts.no_rebuild) {
            REPLACE_TODO.erase(base);
        }

        for (auto const& base: FAILED) {
            REPLACE_TODO.erase(base);
        }
    }

    void
    rolling_replacer::dump_todo() const {
        auto out = env.verbose();

        if (opts.just_fetch) {
            out << "Packages to fetch:" << std::endl;
            dump_todo(out, "MISMATCH_TODO", MISMATCH_TODO);
            dump_todo(out, "MISSING_TODO" , MISSING_TODO );
        }
        else {
            out << "Packages to rebuild:" << std::endl;
            dump_todo(out, "MISMATCH_TODO", MISMATCH_TODO);
            dump_todo(out, "REBUILD_TODO" , REBUILD_TODO );
            dump_todo(out, "MISSING_TODO" , MISSING_TODO );
            dump_todo(out, "UNSAFE_TODO"  , UNSAFE_TODO  );
        }

        env.vsleep(2s);
    }

    void
    rolling_replacer::dump_todo(
        pkgxx::ttystream_base& out, std::string const& label, todo_type const& todo) const {

        // We could simply list packages in the alphabetical order, but
        // here we tsort them so that packages that are going to be
        // replaced soon will appear at the end.
        std::size_t num_pkgs = 0;
        out << label << "=[";
        try {
            bool is_first = true;
            for (auto const& base: pkgxx::reverse(topology.tsort(true))) {
                if (auto it = todo.find(base); it != todo.end()) {
                    if (is_first) {
                        is_first = false;
                    }
                    else {
                        out << ' ';
                    }
                    out << (num_pkgs % 2 == 0 ? _even_sty : _odd_sty)(it->first);
                    num_pkgs++;
                }
            }
        }
        catch (pkgxx::not_a_dag<pkgxx::pkgbase>& e) {
            // We found a cycle in the dependency graph. Letting it go down
            // the stack is not nice, because we are in the middle of
            // printing package names in a single line.
            out << std::endl;
            abort([&](auto& out) {
                      out << "Found a cycle in the dependency graph: "
                          << e.cycle() << std::endl;
                  });
        }
        out << ']';
        if (num_pkgs > 0) {
            out << " (" << tty::dull_colour(tty::yellow)(num_pkgs) << ' '
                << (num_pkgs == 1 ? "package" : "packages")
                << ')';
        }
        out << std::endl;
    }

    bool
    rolling_replacer::is_pkg_installed(pkgxx::pkgbase const& base) const {
        // pkg_rr never deinstalls anything. Once we find something's
        // installed, it will never disappear. And no, we cannot
        // support OLDNAME unfortunately, because doing it would mean
        // we have to check each and every package if it's been
        // renamed, before checking for new dependencies. That would
        // take like 30 minutes for mostly nothing.
        if (definitely_installed.count(base)) {
            return true;
        }
        else if (pkgxx::is_pkg_installed(env.PKG_INFO.get(), base)) {
            definitely_installed.insert(base);
            return true;
        }
        else {
            return false;
        }
    }

    pkgxx::graph<pkgxx::pkgbase, void, true>
    rolling_replacer::depgraph_installed() const {
        env.msg() << "Building dependency graph for installed packages" << std::endl;

        // There are two ways to build it. First, enumerate all the
        // installed packages and see which packages they depend
        // on. Second, begin with packages listed in REPLACE_TODO and
        // recursively discover dependencies until reaching roots. The
        // former is far easier to implement but is more costly than
        // the latter. We do the latter here.
        auto const& PKG_INFO = env.PKG_INFO.get();
        pkgxx::guarded<
            decltype(depgraph_installed())
            > depgraph;

        std::set<pkgxx::pkgbase> to_scan;
        for (auto const& [base, _path]: REPLACE_TODO) {
            to_scan.insert(base);
        }

        while (!to_scan.empty()) {
            pkgxx::guarded<decltype(to_scan)> scheduled;
            {
                // Breadth-first search to increase concurrency.
                pkgxx::nursery n(opts.concurrency);
                for (auto const& base: to_scan) {
                    // Note that packages in "scheduled" might not be
                    // actually installed. This can happen when a
                    // build-only dependency has been deinstalled after
                    // building packages. It's perfectly okay, as we'll
                    // later discover dependencies of such packages in the
                    // "new depends" phase.
                    if (!is_pkg_installed(base))
                        continue;

                    n.start_soon(
                        // Don't need to move-capture 'base' because it
                        // is guaranteed to outlive the closure.
                        [&]() {
                            // This operation is expensive so we shouldn't
                            // lock guarded values while doing it.
                            auto const& deps = pkgxx::build_depends(PKG_INFO, base);
                            if (deps.empty()) {
                                // A package may have no dependencies at
                                // all. Add at least a vertex in that case,
                                // or we will fail to update it.
                                depgraph.lock()->add_vertex(base);
                            }
                            else {
                                // We don't want to repeatedly lock and
                                // unlock them.
                                auto&& depgraph_g  = depgraph.lock();
                                auto&& scheduled_g = scheduled.lock();
                                for (auto const& dep: deps) {
                                    if (!depgraph_g->has_vertex(dep.base)) {
                                        scheduled_g->insert(dep.base);
                                    }
                                    depgraph_g->add_edge(base, dep.base);
                                }
                            }
                        });
                }
            }
            to_scan = std::move(*(scheduled.lock()));
        }

        // Now we have a graph of @blddep entries, which includes not only
        // BUILD_DEPENDS and DEPENDS but also BOOTSTRAP_DEPENDS. The
        // problem is that FETCH_USING also shows up in BOOTSTRAP_DEPENDS
        // and creates cycles, so we must remove every edge that goes into
        // it. Don't worry, if anything BUILD_DEPENDS or DEPENDS on it,
        // such edges will be discovered later in the "new depends" phase.
        if (auto const FETCH_USING = env.FETCH_USING.get(); FETCH_USING) {
            depgraph.lock()->remove_in_edges(FETCH_USING.value());
        }

        return std::move(*(depgraph.lock()));
    }

    std::pair<pkgxx::pkgbase, pkgxx::pkgpath>
    rolling_replacer::choose_one() const {
        for (auto const& base: topology.tsort(true)) {
            if (auto it = REPLACE_TODO.find(base); it != REPLACE_TODO.end()) {
                return *it;
            }
        }
        assert("Internal inconsistency: cannot choose one" && false);
    }

    pkgxx::pkgversion
    rolling_replacer::update_depends_with_source(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) {
        env.msg() << "Checking if " << _pkgname_sty(base) << " has new depends..." << std::endl;
        auto const old_depends = topology.out_edges(base).value();
        auto const source      = source_depends(base, path);
        auto const& [version, new_depends] = source;

        if (depends_differ(old_depends, new_depends)) {
            dump_new_depends(base, old_depends, new_depends);
            topology.remove_out_edges(base); // This invalidates old_depends!

            bool something_is_missing = false;
            for (auto const& dep: new_depends) {
                auto const& [dep_base, _dep_path] = dep;
                topology.add_edge(base, dep_base);
                if (!is_pkg_installed(dep_base)) {
                    // This dependency isn't installed yet, and we don't
                    // know which packages it depends on. We will need to
                    // discover that later.
                    MISSING_TODO.insert(dep);
                    something_is_missing = true;
                }
            }

            if (something_is_missing) {
                refresh_todo();
                dump_todo();
            }
        }

        return std::move(version);
    }

    bool
    rolling_replacer::depends_differ(
        std::set<
            std::reference_wrapper<pkgxx::pkgbase const>,
            std::less<pkgxx::pkgbase>
            > const& old_depends,
        std::map<pkgxx::pkgbase, pkgxx::pkgpath> const& new_depends) {

        if (old_depends.size() != new_depends.size()) {
            return true;
        }
        else {
            for (auto const& dep_base: old_depends) {
                if (new_depends.count(dep_base) == 0) {
                    return true;
                }
            }
            for (auto const& [dep_base, _dep_path]: new_depends) {
                if (old_depends.count(dep_base) == 0) {
                    return true;
                }
            }
            return false;
        }
    }

    void
    rolling_replacer::dump_new_depends(
        pkgxx::pkgbase const& base,
        std::set<
            std::reference_wrapper<pkgxx::pkgbase const>,
            std::less<pkgxx::pkgbase>
            > const& old_depends,
        std::map<pkgxx::pkgbase, pkgxx::pkgpath> const& new_depends) {

        auto out = env.msg();
        bool is_first = true;
        for (auto const& [dep_base, _pair]: new_depends) {
            if (old_depends.count(dep_base) == 0) {
                if (is_first) {
                    out << _pkgname_sty(base) << " has the following new depends (need to re-tsort):" << std::endl
                        << '[';
                    is_first = false;
                }
                else {
                    out << ' ';
                }
                out << _new_deps_sty(dep_base);
            }
        }
        if (!is_first) {
            out << ']' << std::endl;
            env.vsleep(2s);
        }
    }

    std::map<std::string, std::string>
    rolling_replacer::make_vars_for_pkg(pkgxx::pkgbase const& base) const {
        // Set PKGNAME_REQD to give underlying make processes a chance
        // to set options derived from the package name.  For example,
        // the appropriate version of Python can be derived from the
        // package name (so, when building py34-foo, use python-3.4,
        // not python-2.7).
        auto ret = opts.make_vars;
        ret["PKGNAME_REQD"] = base + "-[0-9]*";
        return ret;
    }

    void
    rolling_replacer::run_make(
        pkgxx::pkgbase const& base,
        pkgxx::pkgpath const& path,
        std::initializer_list<std::string> const& targets,
        std::map<std::string, std::string> const& vars) const {

        auto const& pkgdir = env.PKGSRCDIR.get() / path;
        if (!fs::exists(pkgdir / "Makefile")) {
            throw replace_failed("Makefile is missing from " + pkgdir.string());
        }

        std::vector<std::string> argv = {
            CFG_BMAKE, "-C", pkgdir.string()
        };
        for (auto const& target: targets) {
            argv.push_back(target);
        }
        for (auto const& [var, value]: vars) {
            argv.push_back(var + '=' + value);
        }

        if (opts.dry_run) {
            env.msg() << tty::faint("Would run: "_ch << pkgxx::stringify_argv(argv))
                      << std::endl;
        }
        else if (opts.log_dir) {
            auto const version  = DEPENDS_CHECKED.find(base);
            assert(version != DEPENDS_CHECKED.end());
            auto const log_dir  = *opts.log_dir / static_cast<fs::path>(path).parent_path();
            auto const log_file = log_dir / pkgxx::pkgname(base, version->second).string();
            fs::create_directories(log_dir);

            std::ofstream log_out(log_file, std::ios_base::app);
            if (!log_out) {
                throw std::system_error(
                    errno, std::generic_category(), "Failed to open " + log_file.string());
            }
            log_out.exceptions(std::ios_base::badbit);

            using namespace na::literals;
            pkgxx::harness make(
                CFG_BMAKE, argv,
                "stdin_action"_na  = pkgxx::harness::fd_action::inherit,
                "stdout_action"_na = pkgxx::harness::fd_action::pipe,
                "stderr_action"_na = pkgxx::harness::fd_action::merge_with_stdout);

            std::array<char, 1024> buf;
            using traits = std::decay_t<decltype(make.cin())>::traits_type;
            while (true) {
                if (auto const n_read = make.cout().readsome(buf.data(), buf.size()); n_read > 0) {
                    std::cout.write(buf.data(), n_read);
                    log_out.write(buf.data(), n_read);
                }
                else if (traits::eq_int_type(make.cout().peek(), traits::eof())) {
                    // There were no characters readily available, and even
                    // peek() could not get more.
                    break;
                }
            }

            if (make.wait_exit().status != 0) {
                throw replace_failed("Command failed: " + pkgxx::stringify_argv(argv));
            }
        }
        else {
            using namespace na::literals;
            pkgxx::harness make(
                CFG_BMAKE, argv,
                "stdin_action"_na  = pkgxx::harness::fd_action::inherit,
                "stdout_action"_na = pkgxx::harness::fd_action::inherit,
                "stderr_action"_na = pkgxx::harness::fd_action::inherit);

            if (make.wait_exit().status != 0) {
                throw replace_failed("Command failed: " + pkgxx::stringify_argv(argv));
            }
        }
    }

    pkgxx::harness
    rolling_replacer::spawn_su(std::string const& cmd) const {
        std::vector<std::string> argv = {
            pkgxx::shell, "-c"
        };
        if (env.SU_CMD.get().empty()) {
            argv.push_back("exec " + cmd);
        }
        else {
            // SU_CMD expects that only a single argument is given. The
            // argument is interpreted as a POSIX shell script.
            argv.push_back("exec " + env.SU_CMD.get() + " \"$0\"");
            argv.push_back(cmd);
        }

        using namespace na::literals;
        return pkgxx::harness(
            pkgxx::shell, argv,
            "stdin_action"_na  = pkgxx::harness::fd_action::pipe,
            "stdout_action"_na = pkgxx::harness::fd_action::inherit,
            "stderr_action"_na = pkgxx::harness::fd_action::inherit);
    }

    std::pair<
        pkgxx::pkgversion,
        std::map<pkgxx::pkgbase, pkgxx::pkgpath>
        >
    rolling_replacer::source_depends(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) const {
        auto const pkgdir = env.PKGSRCDIR.get() / path;
        auto vars =
            pkgxx::extract_pkgmk_vars(
                pkgdir,
                {"PKGVERSION", "BUILD_DEPENDS", "TOOL_DEPENDS", "DEPENDS"},
                make_vars_for_pkg(base));
        if (!vars.has_value()) {
            throw replace_failed("Makefile is missing from " + pkgdir.string());
        }

        auto it = vars->find("PKGVERSION");
        assert(it != vars->end());
        auto const version = pkgxx::pkgversion(it->second);
        vars->erase(it);

        std::unordered_map<pkgxx::pkgpattern, pkgxx::pkgpath> deps;
        for (auto const& [var, value]: *vars) {
            for (auto const& dep: pkgxx::words(value)) {
                if (auto colon = dep.find(':'); colon != std::string_view::npos) {
                    auto dep_pattern = dep.substr(0, colon);
                    auto dep_path    = dep.substr(colon + 1);

                    if (dep_path.substr(0, 6) == "../../") {
                        deps.emplace(
                            pkgxx::pkgpattern(dep_pattern),
                            pkgxx::pkgpath(dep_path.substr(6)));
                        continue;
                    }
                }
                env.warn() << "Invalid dependency: `" << dep << "' in " << var << std::endl;
            }
        }

        // Now we need to extract a PKGBASE out of the pattern. In the
        // general case we have to consult pkgsrc, which is seriously a
        // constly operation. But if the pattern is a simple version
        // constraint (e.g. foo>=1.2<3) we can cheat it because PKGBASE is
        // already in the pattern. Note that we can't safely do the same
        // for glob patterns like "foo-[0-9]*", because it's possible,
        // although highly unlikely, that it is intended to match something
        // like "foo-0-bar-1.2nb3".
        pkgxx::guarded<
            std::map<pkgxx::pkgbase, pkgxx::pkgpath>
            > resolved_deps;
        {
            pkgxx::nursery n(opts.concurrency);
            for (auto const& dep: deps) {
                auto const& [dep_pattern, dep_path] = dep;

                if (auto dep_base = pattern_to_base_cache.find(dep);
                    dep_base != pattern_to_base_cache.end()) {

                    resolved_deps.lock()->emplace(dep_base->second, dep_path);
                }
                else if (auto dep_base = obvious_pkgbase_of(dep_pattern); dep_base.has_value()) {
                    pattern_to_base_cache.emplace(dep, *dep_base);
                    resolved_deps.lock()->emplace(*dep_base, dep_path);
                }
                else {
                    // The worst case where we have no choice but to
                    // consult pkgsrc Makefiles. Parallelise them of
                    // course.
                    n.start_soon(
                        [&]() {
                            auto make_vars = opts.make_vars;
                            // Okay capturing structured bindings is a
                            // C++20 extension and we're still at
                            // C++17. But both GCC and Clang support it. Do
                            // not complain!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++20-extensions"
                            make_vars["PKGNAME_REQD"] = dep_pattern.string();
                            auto const& dep_base
                                = pkgxx::extract_pkgmk_var<pkgxx::pkgbase>(
                                    env.PKGSRCDIR.get() / dep_path, "PKGBASE", make_vars);
#pragma GCC diagnostic pop
                            if (dep_base.has_value()) {
                                pattern_to_base_cache.emplace(dep, *dep_base);
                                resolved_deps.lock()->emplace(*dep_base, dep_path);
                            }
                            else {
                                throw replace_failed(
                                    "Cannot retrieve PKGBASE from " + path.string());
                            }
                        });
                }
            }
        }
        return std::make_pair(
            std::move(version),
            std::move(*(resolved_deps.lock())));
    }

    void
    rolling_replacer::fetch(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) {
        env.msg() << "Fetching " << base << std::endl;
        run_make(base, path, {"fetch", "depends-fetch"}, make_vars_for_pkg(base));
    }

    void
    rolling_replacer::replace(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) {
        clean(base, path);

        bool const was_installed = is_pkg_installed(base);
        if (was_installed) {
            env.msg() << "Replacing " << _pkgname_sty(base) << std::endl;
        }
        else {
            env.msg() << "Installing " << _pkgname_sty(base) << std::endl;
        }

        auto make_vars = make_vars_for_pkg(base);
        make_vars["PKGSRC_KEEP_BIN_PKGS"] = opts.just_replace ? "NO" : "YES";

        if (was_installed) {
            run_make(base, path, {"replace"}, make_vars);
        }
        else {
            run_make(base, path, {"install"}, make_vars);
            // If the package wasn't installed before we did, it's clear
            // that the user didn't explicitly ask to install it.
            if (!opts.dry_run)
                run_su(env.PKG_ADMIN.get() + ' ' + pkgxx::stringify_argv(
                           std::initializer_list<std::string> {"set", "automatic=YES", base}));
        }

        clean(base, path);

        if (!opts.dry_run) {
            // Sanity checks: see if the newly installed package has a
            // desired set of flags.
            bool is_automatic = false;
            for (auto const& [var, value]: pkgxx::build_info(env.PKG_INFO.get(), base)) {
                if (var == "automatic" && pkgxx::ci_equal(value, "yes")) {
                    is_automatic = true;
                }
                else if (var == "unsafe_depends_strict" && pkgxx::ci_equal(value, "yes")) {
                    abort([&](auto& out) {
                              out << "package `" << base << "' still has unsafe_depends_strict." << std::endl;
                          });
                }
                else if (var == "unsafe_depends" && pkgxx::ci_equal(value, "yes")) {
                    abort([&](auto& out) {
                              out << "package `" << base << "' still has unsafe_depends." << std::endl;
                          });
                }
                else if (var == "rebuild" && pkgxx::ci_equal(value, "yes")) {
                    abort([&](auto& out) {
                              out << "package `" << base << "' is still requested to be rebuilt." << std::endl;
                          });
                }
                else if (var == "mismatch" && pkgxx::ci_equal(value, "yes")) {
                    abort([&](auto& out) {
                              out << "package `" << base << "' is still a mismatched version." << std::endl;
                          });
                }
            }
            if (!was_installed && !is_automatic) {
                abort([&](auto& out) {
                          out << "package `" << base << "' is not marked as automatically installed." << std::endl;
                      });
            }
        }

        // If we are in the dry-run mode and the package isn't actually
        // installed, we cannot run recheck_unsafe() because it will
        // definitely fail.
        if (!opts.dry_run || is_pkg_installed(base))
            recheck_unsafe(base);
    }

    void
    rolling_replacer::clean(pkgxx::pkgbase const& base,
                            pkgxx::pkgpath const& path) {

        // "make clean" is slow because invoking "make" is slow. It's
        // tempting to simulate what it does in C++. However, doing it
        // properly is suprisingly hard because there are so many
        // controlling knobs (e.g. WRKOBJDIR, WRKDIR_BASENAME, ...) so
        // don't even think of that. We have tried in the past, and failed
        // twice. Also don't forget that packages may define
        // {pre,post}-clean targets.
        run_make(base, path, {"clean"}, opts.make_vars);
    }

    void
    rolling_replacer::report() const {
        if (opts.verbose > 0) {
            for (auto const& base: SUCCEEDED) {
                env.raw_msg() << tty::dull_colour(tty::green)("+ "_ch << base) << std::endl;
            }
            for (auto const& base: FAILED) {
                env.raw_msg() << tty::colour(tty::red)("- "_ch << base) << std::endl;
            }
        }
    }
}
