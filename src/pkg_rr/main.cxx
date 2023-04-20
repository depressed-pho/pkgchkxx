#include <iostream>
#include <type_traits>
#include <unordered_map>

#include <pkgxx/graph.hxx>
#include <pkgxx/harness.hxx>
#include <pkgxx/hash.hxx>
#include <pkgxx/iterable.hxx>
#include <pkgxx/makevars.hxx>
#include <pkgxx/nursery.hxx>
#include <pkgxx/pkgdb.hxx>
#include <pkgxx/unwrap.hxx>

#include "config.h"
#include "environment.hxx"
#include "message.hxx"
#include "scan.hxx"
#include "options.hxx"

using namespace pkg_rr;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {
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

    template <typename OldDeps, typename NewDeps>
    bool
    depends_differ(OldDeps const& old_depends, NewDeps const& new_depends) {
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

    struct rolling_replacer {
        rolling_replacer(std::filesystem::path const& progname_,
                         options const& opts_,
                         environment const& env_)
            : progname(progname_)
            , opts(opts_)
            , env(env_)
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
        run() {
            while (!REPLACE_TODO.empty()) {
                auto [base, path] = choose_one();
                if (DEPENDS_CHECKED.count(base)) {
                    msg() << "Selecting " << base << " ("
                          << static_cast<std::filesystem::path const&>(path).string()
                          << ") as next package to replace";
                    vsleep(opts, 1s);
                }
                else {
                    update_depends(base, path);
                    DEPENDS_CHECKED.insert(base);
                    continue;
                }
                break;//
            }
            msg() << "No more packages to replace; done." << std::endl;
            report();
        }

    private:
        using todo_type = std::map<pkgxx::pkgbase, pkgxx::pkgpath>;

        std::future<todo_type>
        check_mismatch(pkg_rr::package_scanner& scanner) const {
            if (opts.check_for_updates) {
                throw "FIXME: -u not implemented";
            }
            else {
                msg() << "Checking for mismatched installed packages (mismatch=YES)" << std::endl;
                return scanner.add_axis("mismatch", opts.no_check);
            }
        }

        std::future<todo_type>
        check_rebuild(pkg_rr::package_scanner& scanner) const {
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

        std::future<todo_type>
        check_unsafe(pkg_rr::package_scanner& scanner) const {
            if (opts.just_fetch) {
                std::promise<todo_type> res;
                res.set_value({});
                return res.get_future();
            }
            else {
                auto const flag = opts.strict ? "unsafe_depends_strict" : "unsafe_depends";
                msg() << "Checking for unsafe installed packages (" << flag << "=YES)" << std::endl;
                return scanner.add_axis(flag);
            }
        }

        /// Update REPLACE_TODO based on the current contents of other
        /// TODOs.
        void
        refresh_todo() {
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
        dump_todo() const {
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
        dump_todo(std::ostream& out, std::string const& label, todo_type const& todo) const {
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
        is_pkg_installed(pkgxx::pkgbase const& base) {
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

        pkgxx::graph<pkgxx::pkgbase, true>
        depgraph_installed() const {
            msg() << "Building dependency graph for installed packages" << std::endl;

            // There are two ways to build it. First, enumerate all the
            // installed packages and see which packages they depend
            // on. Second, begin with packages listed in REPLACE_TODO and
            // recursively discover dependencies until reaching roots. The
            // former is far easier to implement but is more costly than
            // the latter. We do the latter here.
            auto const& PKG_INFO = env.PKG_INFO.get();
            pkgxx::guarded<
                pkgxx::graph<pkgxx::pkgbase, true>
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
        choose_one() const {
            for (auto const& base: topology.tsort(true)) {
                if (auto it = REPLACE_TODO.find(base); it != REPLACE_TODO.end()) {
                    return *it;
                }
            }
            assert("Internal inconsistency: cannot choose one" && false);
        }

        void
        update_depends(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) {
            msg() << "Checking if " << base << " has new depends..." << std::endl;
            auto const& old_depends = topology.out_edges(base).value();
            auto const& new_depends = source_depends(base, path);

            if (depends_differ(old_depends, new_depends)) {
                dump_new_depends(base, old_depends, new_depends);
                topology.remove_out_edges(base); // This invalidates old_depends!

                bool something_is_missing = false;
                for (auto const& dep: new_depends) {
                    topology.add_edge(base, dep.first);
                    if (!is_pkg_installed(dep.first)) {
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

        template <typename OldDeps, typename NewDeps>
        void
        dump_new_depends(pkgxx::pkgbase const& base,
                         OldDeps const& old_depends,
                         NewDeps const& new_depends) {
            auto out = msg();
            bool is_first = true;
            for (auto const& [dep_base, _dep_pkg]: new_depends) {
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
        make_vars_for_pkg(pkgxx::pkgbase const& base) const {
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
        spawn_make(
            std::filesystem::path const& path,
            std::string const& target,
            std::map<std::string, std::string> const& vars) const {

            std::vector<std::string> argv = {
                CFG_BMAKE, "-C", path.string(), target
            };
            for (auto const& [var, value]: vars) {
                argv.push_back(var + '=' + value);
            }
            pkgxx::harness make(CFG_BMAKE, argv);
            make.cin().close();
            return make;
        }

        std::map<pkgxx::pkgbase, pkgxx::pkgpath>
        source_depends(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) const {
            // Unfortunately pkgsrc doesn't provide a target that shows
            // more than a single *_DEPENDS variable at once.
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
                            auto&& make   = spawn_make(env.PKGSRCDIR.get() / path, "show-depends", make_vars);
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
            // constraint (e.g. foo>=1.2<3) we can cheat it because PKGBASE
            // is already in the pattern. Note that we can't safely do the
            // same for glob patterns like "foo-[0-9]*", because it's
            // possible, although highly unlikely, that it is intended to
            // match something like "foo-0-bar-1.2nb3".
            pkgxx::guarded<
                std::map<pkgxx::pkgbase, pkgxx::pkgpath>
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

        template <typename Function>
        [[noreturn]] void
        abort(Function&& f) const {
            static_assert(std::is_invocable_v<Function&&, std::ostream&>);
            pkg_rr::fatal(
                [&](auto& out) {
                    out << "*** "; f(out);
                    out << "*** Please read the errors listed above, fix the problem," << std::endl
                        << "*** then re-run " << progname.filename().string() << " to continue." << std::endl;
                    report();
                });
        }

        void
        report() const {
            for (auto const& base: SUCCEEDED) {
                verbose(opts) << "+ " << base << std::endl;
            }
            for (auto const& base: FAILED) {
                verbose(opts) << "- " << base << std::endl;
            }
        }

        std::filesystem::path progname;
        options opts;
        environment env;

        todo_type MISMATCH_TODO;
        todo_type REBUILD_TODO;
        todo_type MISSING_TODO;
        todo_type UNSAFE_TODO;
        todo_type REPLACE_TODO;

        std::set<pkgxx::pkgbase> SUCCEEDED;
        std::set<pkgxx::pkgbase> FAILED;

        /* The dependency graph is initially built with installed packages
         * and will be progressively updated when new depends are
         * discovered or new packages are installed. */
        pkgxx::graph<pkgxx::pkgbase, true> topology;

        /* A copy of the dependency graph that is never mutated after its
         * initial construction. We need this only in the dry-run mode. */
        pkgxx::graph<pkgxx::pkgbase, true> initial_topology;

        /* Newer versions in pkgsrc sometimes have different sets of
         * dependencies from that are recorded for installed versions. When
         * this happens, we need to update the graph and re-tsort it. The
         * check must be done once (and only once) for every installed
         * package. */
        std::set<pkgxx::pkgbase> DEPENDS_CHECKED;

        // See a comment in source_depends().
        std::unordered_map<
            std::pair<pkgxx::pkgpattern, pkgxx::pkgpath>,
            pkgxx::pkgbase
            > mutable pattern_to_base_cache;

        // See a comment in is_pkg_installed().
        std::set<pkgxx::pkgbase> definitely_installed;
    };

/*
    todo_type
    packages_w_flag(
        options const& opts,
        environment const& env,
        std::string const& flag,
        std::set<pkgxx::pkgbase> const& exclude = {}) {

        std::future<todo_type> res;
        {
            pkg_rr::package_scanner scanner(env.PKG_INFO.get(), opts.concurrency);
            res = scanner.add_axis(flag, exclude);
        }
        return res.get();
    }
*/
}

int main(int argc, char* argv[]) {
    try {
        options opts(argc, argv);

        if (opts.help) {
            usage(argv[0]);
            return 1;
        }

        environment env(opts);
        rolling_replacer(argv[0], opts, env).run();
    }
    catch (bad_options& e) {
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }
}
