#pragma once

#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <ostream>
#include <set>

#include <pkgxx/build_version.hxx>
#include <pkgxx/pkgname.hxx>
#include <pkgxx/stream.hxx>
#include <pkgxx/summary.hxx>

namespace pkg_chk {
    struct checker_base {
        struct result {
            std::set<pkgxx::pkgpath>                 MISSING_DONE;
            std::map<pkgxx::pkgname, pkgxx::pkgpath> MISSING_TODO;
            std::map<pkgxx::pkgname, pkgxx::pkgpath> MISMATCH_TODO;
        };

        checker_base(
            bool add_missing,
            bool check_build_version,
            unsigned concurrency,
            bool update,
            bool delete_mismatched,
            std::shared_future<std::string> const& PKG_INFO);

        /// Run \c pkg_chk for each package path in \c pkgpaths.
        result
        run(std::set<pkgxx::pkgpath> const& pkgpaths) const;

        /// Run \c pkg_chk for each installed package.
        result
        run() const;

        /// Mark a package as deleted. If you intend to re-invoke run()
        /// after deleting some packages, make sure to mark them as
        /// deleted. Otherwise subsequent call of run() may use a stale
        /// cache and return a wrong result.
        void
        mark_as_deleted(pkgxx::pkgname const& name);

    protected:
        /// Return the set of latest PKGNAMEs provided by a given PKGPATH.
        virtual std::set<pkgxx::pkgname>
        find_latest_pkgnames(pkgxx::pkgpath const& path) const = 0;

        /// Return the build version of a given package, or \c std::nullopt
        /// if no such package exists.
        virtual std::optional<pkgxx::build_version>
        fetch_build_version(pkgxx::pkgname const& name, pkgxx::pkgpath const& path) const = 0;

        /// See if a binary package is available for a given package name.
        virtual bool
        is_binary_available(pkgxx::pkgname const&) const {
            return false;
        }

        virtual void
        atomic_msg(std::function<void (std::ostream&)> const& f) const = 0;

        virtual void
        atomic_warn(std::function<void (std::ostream&)> const& f) const = 0;

        virtual void
        atomic_verbose(std::function<void (std::ostream&)> const& f) const = 0;

        virtual void
        fatal(std::function<void (std::ostream&)> const& f) const = 0;

        bool _add_missing;
        bool _check_build_version;
        unsigned _concurrency;
        bool _update;
        bool _delete_mismatched;

        std::shared_future<std::string>              _PKG_INFO;
        std::shared_future<pkgxx::summary>           _installed_pkg_summary;
        std::shared_future<std::set<pkgxx::pkgname>> _installed_pkgnames;

        std::set<pkgxx::pkgname> _deleted_pkgnames;
    };

    /// Obtains data from source.
    struct source_checker_base: virtual checker_base {
        source_checker_base(
            std::shared_future<std::filesystem::path> const& PKGSRCDIR);

    protected:
        virtual std::set<pkgxx::pkgname>
        find_latest_pkgnames(pkgxx::pkgpath const& path) const override;

        virtual std::optional<pkgxx::build_version>
        fetch_build_version(pkgxx::pkgname const& name, pkgxx::pkgpath const& path) const override;

        std::shared_future<std::filesystem::path> _PKGSRCDIR;
        std::shared_future<
            std::map<
                pkgxx::pkgpath,
                std::set<pkgxx::pkgname>
                >
            > _installed_pkgpaths_with_pkgnames;
    };

    /// Obtains data from a binary package repository.
    struct binary_checker_base: virtual checker_base {
        binary_checker_base(
            std::shared_future<std::filesystem::path> const& PACKAGES,
            std::shared_future<std::string> const& PKG_SUFX,
            std::shared_future<pkgxx::summary> const& bin_pkg_summary);

    protected:
        virtual std::set<pkgxx::pkgname>
        find_latest_pkgnames(pkgxx::pkgpath const& path) const override;

        virtual std::optional<pkgxx::build_version>
        fetch_build_version(pkgxx::pkgname const& name, pkgxx::pkgpath const& path) const override;

        virtual bool
        is_binary_available(pkgxx::pkgname const& name) const override {
            return _bin_pkg_summary.get().count(name) > 0;
        }

        std::optional<std::filesystem::path>
        binary_package_file_of(pkgxx::pkgname const& name) const;

        std::shared_future<std::filesystem::path> _PACKAGES;
        std::shared_future<std::string>           _PKG_SUFX;
        std::shared_future<pkgxx::summary>        _bin_pkg_summary;
        std::shared_future<pkgxx::pkgmap>         _bin_pkg_map;
    };

    /// Obtains data from either source or binary, configurable at run
    /// time.
    struct configurable_checker_base: virtual source_checker_base
                                    , virtual binary_checker_base {
        configurable_checker_base(bool use_source)
            : _use_source(use_source) {}

    protected:
        virtual std::set<pkgxx::pkgname>
        find_latest_pkgnames(pkgxx::pkgpath const& path) const override {
            return _use_source
                ? source_checker_base::find_latest_pkgnames(path)
                : binary_checker_base::find_latest_pkgnames(path);
        }

        virtual std::optional<pkgxx::build_version>
        fetch_build_version(pkgxx::pkgname const& name, pkgxx::pkgpath const& path) const override {
            return _use_source
                ? source_checker_base::fetch_build_version(name, path)
                : binary_checker_base::fetch_build_version(name, path);
        }

        bool _use_source;
    };
}
