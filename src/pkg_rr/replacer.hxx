#pragma once

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
#include "scanner.hxx"
#include "options.hxx"

namespace pkg_rr {
    struct rolling_replacer {
        rolling_replacer(
            std::filesystem::path const& progname_,
            options const& opts_,
            environment const& env_);

        void
        run();

    private:
        using todo_type = std::map<pkgxx::pkgbase, pkgxx::pkgpath>;

        std::future<todo_type>
        check_mismatch(pkg_rr::package_scanner& scanner) const;

        std::future<todo_type>
        check_rebuild(pkg_rr::package_scanner& scanner) const;

        std::future<todo_type>
        check_unsafe(pkg_rr::package_scanner& scanner) const;

        /// Update REPLACE_TODO based on the current contents of other
        /// TODOs.
        void
        refresh_todo();

        void
        dump_todo() const;

        void
        dump_todo(std::ostream& out, std::string const& label, todo_type const& todo) const;

        bool
        is_pkg_installed(pkgxx::pkgbase const& base);

        pkgxx::graph<pkgxx::pkgbase, true>
        depgraph_installed() const;

        std::pair<pkgxx::pkgbase, pkgxx::pkgpath>
        choose_one() const;

        void
        update_depends_with_source(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path);

        void
        dump_new_depends(
            pkgxx::pkgbase const& base,
            std::set<
                std::reference_wrapper<pkgxx::pkgbase const>,
                std::less<pkgxx::pkgbase>
                > const& old_depends,
            std::map<pkgxx::pkgbase, pkgxx::pkgpath> const& new_depends);

        std::map<std::string, std::string>
        make_vars_for_pkg(pkgxx::pkgbase const& base) const;

        pkgxx::harness
        spawn_make(
            std::filesystem::path const& path,
            std::string const& target,
            std::map<std::string, std::string> const& vars) const;

        std::map<pkgxx::pkgbase, pkgxx::pkgpath>
        source_depends(pkgxx::pkgbase const& base, pkgxx::pkgpath const& path) const;

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
        report() const;

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
}
