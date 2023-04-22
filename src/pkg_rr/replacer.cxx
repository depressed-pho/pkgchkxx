#include <exception>
#include <filesystem>

#include <pkgxx/string_algo.hxx>

#include "replacer.hxx"

using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {
    struct replace_failed: std::runtime_error {
        using std::runtime_error::runtime_error;
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

    std::vector<std::string>
    make_argv(fs::path const& PKGSRCDIR,
              pkgxx::pkgpath const& path,
              std::initializer_list<std::string> const& targets,
              std::map<std::string, std::string> const& vars) {

        auto const& abspath   = PKGSRCDIR / path;
        if (!fs::is_directory(abspath)) {
            throw replace_failed(
                "No package directory `" + path.string() + "' in " + PKGSRCDIR.string());
        }

        std::vector<std::string> argv = {
            CFG_BMAKE, "-C", abspath.string(),
        };
        for (auto const& target: targets) {
            argv.push_back(target);
        }
        for (auto const& [var, value]: vars) {
            argv.push_back(var + '=' + value);
        }
        return argv;
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
        , pattern_to_base_cache(0) {

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
                    msg() << "Selecting " << base << " ("
                          << static_cast<std::filesystem::path const&>(path).string()
                          << ") as next package to replace" << std::endl;
                    vsleep(opts, 1s);
                }
                else {
                    update_depends_with_source(base, path);
                    DEPENDS_CHECKED.insert(base);
                    continue;
                }

                if (opts.just_fetch) {
                    fetch(base, path);
                }
                else {
                    replace(base, path);
                }
                SUCCEEDED.insert(base);
            }
            catch (replace_failed const& e) {
                FAILED.insert(base);
                auto const& cb = [&](auto& out) { out << e.what() << std::endl; };
                if (opts.continue_on_errors) {
                    error(cb);
                }
                else {
                    abort(cb);
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
            vsleep(opts, 2s);
        }
        msg() << "No more packages to replace; done." << std::endl;
        report();
    }

    std::future<rolling_replacer::todo_type>
    rolling_replacer::check_mismatch(pkg_rr::package_scanner& scanner) const {
        if (opts.check_for_updates) {
            throw "FIXME: -u not implemented";
        }
        else {
            msg() << "Checking for mismatched installed packages (mismatch=YES)" << std::endl;
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
            msg() << "Checking for rebuild-requested installed packages (rebuild=YES)" << std::endl;
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
            msg() << "Checking for unsafe installed packages (" << UNSAFE_VAR << "=YES)" << std::endl;
            return scanner.add_axis(UNSAFE_VAR);
        }
    }

    void
    rolling_replacer::recheck_unsafe(pkgxx::pkgbase const& base) {
        msg() << "Re-checking for unsafe installed packages (" << UNSAFE_VAR << "=YES)" << std::endl;
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
                            auto build_info  = pkgxx::build_info(PKG_INFO, unsafe_pkg.base);
                            auto unsafe_path = build_info.find("PKGPATH");
                            assert(unsafe_path != build_info.end());
                            unsafe_pkgs.lock()->emplace(unsafe_pkg.base, unsafe_path->second);
                        });
                }
                else {
                    n.start_soon(
                        [&, unsafe_pkg = std::move(unsafe_pkg)]() {
                            std::optional<pkgxx::pkgpath> unsafe_path;
                            for (auto const& [var, value]: pkgxx::build_info(PKG_INFO, unsafe_pkg.base)) {
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
        if (opts.just_fetch) {
            verbose(opts, [&](auto& out) {
                              out << "Packages to fetch:" << std::endl;
                              dump_todo(out, "MISMATCH_TODO", MISMATCH_TODO);
                              dump_todo(out, "MISSING_TODO" , MISSING_TODO );
                          });
        }
        else {
            verbose(opts, [&](auto& out) {
                              out << "Packages to rebuild:" << std::endl;
                              dump_todo(out, "MISMATCH_TODO", MISMATCH_TODO);
                              dump_todo(out, "REBUILD_TODO" , REBUILD_TODO );
                              dump_todo(out, "MISSING_TODO" , MISSING_TODO );
                              dump_todo(out, "UNSAFE_TODO"  , UNSAFE_TODO  );
                          });
        }
        vsleep(opts, 2s);
    }

    void
    rolling_replacer::dump_todo(
        std::ostream& out, std::string const& label, todo_type const& todo) const {

        // We could simply list packages in the alphabetical order, but
        // here we tsort them so that packages that are going to be
        // replaced soon will appear at the end.
        out << label << "=[";
        bool is_first = true;
        for (auto const& base: pkgxx::reverse(topology.tsort(true))) {
            if (auto it = todo.find(base); it != todo.end()) {
                if (is_first) {
                    is_first = false;
                }
                else {
                    out << ' ';
                }
                out << it->first;
            }
        }
        out << ']' << std::endl;
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
        msg() << "Building dependency graph for installed packages" << std::endl;

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
                    n.start_soon(
                        // Don't need to move-capture 'base' because it
                        // is guaranteed to outlive the closure.
                        [&]() {
                            // This operation is expensive so we shouldn't
                            // lock guarded values while doing it.
                            auto const& deps = pkgxx::build_depends(PKG_INFO, base);

                            // And of course we don't want to repeatedly
                            // lock and unlock them.
                            auto&& depgraph_g  = depgraph.lock();
                            auto&& scheduled_g = scheduled.lock();
                            for (auto const& dep: deps) {
                                if (!depgraph_g->has_vertex(dep.base)) {
                                    scheduled_g->insert(dep.base);
                                }
                                depgraph_g->add_edge(base, dep.base);
                            }
                        });
                }
            }
            to_scan = std::move(*(scheduled.lock()));
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

    void
    rolling_replacer::update_depends_with_source(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) {
        msg() << "Checking if " << base << " has new depends..." << std::endl;
        auto const old_depends = topology.out_edges(base).value();
        auto const new_depends = source_depends(base, path);

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

        auto out = msg();
        bool is_first = true;
        for (auto const& [dep_base, _pair]: new_depends) {
            if (old_depends.count(dep_base) == 0) {
                if (is_first) {
                    out << base << " has the following new depends (need to re-tsort):" << std::endl
                        << '[';
                    is_first = false;
                }
                else {
                    out << ' ';
                }
                out << dep_base;
            }
        }
        if (!is_first) {
            out << ']' << std::endl;
            vsleep(opts, 2s);
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

    pkgxx::harness
    rolling_replacer::spawn_make(
        pkgxx::pkgpath const& path,
        std::initializer_list<std::string> const& targets,
        std::map<std::string, std::string> const& vars) const {

        pkgxx::harness make(CFG_BMAKE, make_argv(env.PKGSRCDIR.get(), path, targets, vars));
        make.cin().close();
        return make;
    }

    void
    rolling_replacer::run_make(
        pkgxx::pkgpath const& path,
        std::initializer_list<std::string> const& targets,
        std::map<std::string, std::string> const& vars) const {

        auto const& argv = make_argv(env.PKGSRCDIR.get(), path, targets, vars);
        if (opts.dry_run) {
            msg([&](auto& out) {
                    out << "Would run:";
                    for (auto const& arg: argv) {
                        out << ' ' << arg;
                    }
                    out << std::endl;
                });
        }
        else {
            throw replace_failed("FIXME: not implemented: non-dry-run");
        }
    }

    std::map<pkgxx::pkgbase, pkgxx::pkgpath>
    rolling_replacer::source_depends(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) const {
        // Unfortunately pkgsrc doesn't provide a target that shows more
        // than a single *_DEPENDS variable at once.
        pkgxx::guarded<
            std::unordered_map<pkgxx::pkgpattern, pkgxx::pkgpath>
            > deps;
        {
            pkgxx::nursery n(opts.concurrency);
            for (auto const& var: {"BUILD_DEPENDS", "TOOL_DEPENDS", "DEPENDS"}) {
                n.start_soon(
                    [&]() {
                        auto&& make_vars = make_vars_for_pkg(base);
                        make_vars["VARNAME"] = var;
                        auto&& make   = spawn_make(path, {"show-depends"}, make_vars);
                        auto&& deps_g = deps.lock();
                        for (std::string line; std::getline(make.cout(), line); ) {
                            if (auto colon = line.find(':'); colon != std::string::npos) {
                                std::string_view const line_v = line;
                                auto dep_pattern = line_v.substr(0, colon);
                                auto dep_path    = line_v.substr(colon + 1);
                                assert(dep_path.substr(0, 6) == "../../");
                                deps_g->emplace(
                                    pkgxx::pkgpattern(dep_pattern),
                                    pkgxx::pkgpath(dep_path.substr(6)));
                            }
                        }
                    });
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
            decltype(source_depends(base, path))
            > ret;
        {
            pkgxx::nursery n(opts.concurrency);
            for (auto const& dep: *(deps.lock())) {
                auto const& [dep_pattern, dep_path] = dep;

                if (auto dep_base = pattern_to_base_cache.find(dep);
                    dep_base != pattern_to_base_cache.end()) {

                    ret.lock()->emplace(dep_base->second, dep_path);
                }
                else if (auto dep_base = obvious_pkgbase_of(dep_pattern); dep_base.has_value()) {
                    pattern_to_base_cache.emplace(dep, *dep_base);
                    ret.lock()->emplace(*dep_base, dep_path);
                }
                else {
                    // The worst case where we have no choice but to
                    // consult pkgsrc Makefiles. Parallelise them of
                    // course.
                    n.start_soon(
                        [&]() {
                            auto make_vars = opts.make_vars;
                            make_vars["PKGNAME_REQD"] = dep_pattern.string();
                            auto const& dep_base
                                = pkgxx::extract_pkgmk_var<pkgxx::pkgbase>(
                                    env.PKGSRCDIR.get() / dep_path, "PKGBASE", make_vars);
                            if (dep_base.has_value()) {
                                pattern_to_base_cache.emplace(dep, *dep_base);
                                ret.lock()->emplace(*dep_base, dep_path);
                            }
                            else {
                                abort(
                                    [&](auto& out) {
                                        out << "Cannot retrieve PKGBASE from " << path << std::endl;
                                    });
                            }
                        });
                }
            }
        }
        return std::move(*(ret.lock()));
    }

    void
    rolling_replacer::fetch(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) {
        msg() << "Fetching " << base << std::endl;
        run_make(path, {"fetch", "depends-fetch"}, make_vars_for_pkg(base));
    }

    void
    rolling_replacer::replace(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) {
        bool const was_installed = is_pkg_installed(base);
        if (was_installed) {
            msg() << "Replacing " << base << std::endl;
        }
        else {
            msg() << "Installing " << base << std::endl;
        }

        auto make_vars = make_vars_for_pkg(base);
        make_vars["PKGSRC_KEEP_BIN_PKGS"] = opts.just_replace ? "NO" : "YES";
        if (!was_installed) {
            // If the package wasn't installed before we did, it's clear
            // that the user didn't explicitly ask to install it. It's not
            // nice to directly manipulate an internal variable here, but
            // there is no better way to achieve this aside from doing a
            // SU_CMD dance (which we really hate to do).
            make_vars["_AUTOMATIC"] = "YES";
        }

        run_make(path, {"clean"}, opts.make_vars);
        if (was_installed) {
            run_make(path, {"replace"}, make_vars);
        }
        else {
            run_make(path, {"install"}, make_vars);
        }
        run_make(path, {"clean"}, opts.make_vars);

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

        recheck_unsafe(base);
    }

    void
    rolling_replacer::report() const {
        if (opts.verbose > 0) {
            for (auto const& base: SUCCEEDED) {
                std::cout << "+ " << base << std::endl;
            }
            for (auto const& base: FAILED) {
                std::cout << "- " << base << std::endl;
            }
        }
    }
}
