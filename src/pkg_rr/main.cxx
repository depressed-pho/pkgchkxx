#include <iostream>

#include <pkgxx/graph.hxx>
#include <pkgxx/nursery.hxx>
#include <pkgxx/pkgdb.hxx>

#include "environment.hxx"
#include "message.hxx"
#include "scan.hxx"
#include "options.hxx"

using namespace pkg_rr;
namespace fs = std::filesystem;

namespace {
    struct rolling_replacer {
        using todo_type = std::map<pkgxx::pkgbase, pkgxx::pkgpath>;

        rolling_replacer(options const& opts_, environment const& env_)
            : opts(opts_)
            , env(env_) {

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
            refresh();

            if (!REPLACE_TODO.empty()) {
                DEPGRAPH = depgraph_installed();
            }
        }

        void
        run() {
            
        }

    private:
        std::future<todo_type>
        check_mismatch(pkg_rr::package_scanner& scanner) {
            if (opts.check_for_updates) {
                throw "FIXME: -u not implemented";
            }
            else {
                msg() << "Checking for mismatched installed packages (mismatch=YES)" << std::endl;
                return scanner.add_axis("mismatch", opts.no_check);
            }
        }

        std::future<todo_type>
        check_rebuild(pkg_rr::package_scanner& scanner) {
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
        check_unsafe(pkg_rr::package_scanner& scanner) {
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
        refresh() {
            if (opts.just_fetch) {
                auto out = verbose(opts);
                out << "Packages to fetch:" << std::endl
                    << "MISMATCH_TODO=";
                dump_todo(out, MISMATCH_TODO);
                out << "MISSING_TODO=";
                dump_todo(out, MISSING_TODO);
                out << std::endl;

                REPLACE_TODO = MISMATCH_TODO;
                REPLACE_TODO.insert(MISSING_TODO.begin(), MISSING_TODO.end());
            }
            else {
                auto out = verbose(opts);
                out << "Packages to rebuild:" << std::endl
                    << "MISMATCH_TODO=";
                dump_todo(out, MISMATCH_TODO);
                out << std::endl
                    << "REBUILD_TODO=";
                dump_todo(out, REBUILD_TODO);
                out << std::endl
                    << "MISSING_TODO=";
                dump_todo(out, MISSING_TODO);
                out << std::endl
                    << "UNSAFE_TODO=";
                dump_todo(out, UNSAFE_TODO);
                out << std::endl;

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
        dump_todo(std::ostream& out, todo_type const& todo) {
            // FIXME: tsort the TODO dump.
            out << '[';
            bool is_first = true;
            for (auto const& [pkg, _path]: todo) {
                if (is_first) {
                    is_first = false;
                }
                else {
                    out << ' ';
                }
                out << pkg;
            }
            out << ']';
        }

        pkgxx::graph<pkgxx::pkgbase, true>
        depgraph_installed() {
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
                    pkgxx::nursery n(opts.concurrency);
                    for (auto const& base: to_scan) {
                        n.start_soon(
                            [&]() {
                                // This operation is expensive so we shouldn't
                                // lock guarded values while doing it.
                                auto&& deps = pkgxx::build_depends(PKG_INFO, base);

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
                to_scan = *(scheduled.lock());
            }

            return std::move(*(depgraph.lock()));
        }

        options opts;
        environment env;

        todo_type MISMATCH_TODO;
        todo_type REBUILD_TODO;
        todo_type MISSING_TODO;
        todo_type UNSAFE_TODO;
        todo_type REPLACE_TODO;
        std::set<pkgxx::pkgbase> FAILED;

        /* DEPGRAPH is initially built with installed packages and will be
         * progressively updated when new depends are discovered or new
         * packages are installed. */
        pkgxx::graph<pkgxx::pkgbase, true> DEPGRAPH;
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
        rolling_replacer(opts, env).run();
    }
    catch (bad_options& e) {
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }
}
