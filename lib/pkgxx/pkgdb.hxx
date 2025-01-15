#pragma once

#include <map>
#include <set>

#include <pkgxx/pkgname.hxx>
#include <pkgxx/pkgpattern.hxx>

namespace pkgxx {
    namespace detail {
        std::map<std::string, std::string>
        build_info(std::string const& PKG_INFO, pkgxx::pkgpattern const& name);

        bool
        is_pkg_installed(std::string const& PKG_INFO, pkgxx::pkgpattern const& pat);

        std::set<pkgxx::pkgname>
        build_depends(std::string const& PKG_INFO, pkgxx::pkgpattern const& pat);

        std::set<pkgxx::pkgname>
        who_requires(std::string const& PKG_INFO, pkgxx::pkgpattern const& pat);
    }

    /// Obtain the set of installed package names. This function is
    /// obviously not efficient. Use it sparingly.
    std::set<pkgxx::pkgname>
    installed_pkgnames(std::string const& PKG_INFO);

    /// Obtain the map of build information for a package. \c Name must
    /// either be a \ref pkgxx::pkgbase or \ref pkgxx::pkgname.
    template <typename Name>
    inline std::map<std::string, std::string>
    build_info(std::string const& PKG_INFO, Name const& name) {
        return detail::build_info(PKG_INFO, pkgxx::pkgpattern(name));
    }

    /// Check if a package is installed. \c Name must either be a \ref
    /// pkgxx::pkgbase or \ref pkgxx::pkgname.
    template <typename Name>
    inline bool
    is_pkg_installed(std::string const& PKG_INFO, Name const& name) {
        return detail::is_pkg_installed(PKG_INFO, pkgxx::pkgpattern(name));
    }

    /// Obtain the set of \c \@blddep entries of an installed package. \c
    /// Name must either be a \ref pkgxx::pkgbase or \ref
    /// pkgxx::pkgname. This includes \c BOOTSTRAP_DEPENDS, \c
    /// BUILD_DEPENDS, and \c DEPENDS but not \c TOOL_DEPENDS.
    template <typename Name>
    inline std::set<pkgxx::pkgname>
    build_depends(std::string const& PKG_INFO, Name const& name) {
        return detail::build_depends(PKG_INFO, pkgxx::pkgpattern(name));
    }

    /// Obtain the set of packages which has a run-time dependency on the
    /// given one. \c Name must either be a \ref pkgxx::pkgbase or \ref
    /// pkgxx::pkgname.
    template <typename Name>
    inline std::set<pkgxx::pkgname>
    who_requires(std::string const& PKG_INFO, Name const& name) {
        return detail::who_requires(PKG_INFO, pkgxx::pkgpattern(name));
    }
}
