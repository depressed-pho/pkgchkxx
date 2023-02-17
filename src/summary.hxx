#pragma once

#include <filesystem>
#include <istream>
#include <map>
#include <set>

#include "options.hxx"
#include "pkgpath.hxx"
#include "pkgpattern.hxx"
#include "pkgname.hxx"

namespace pkg_chk {
    /** pkg_summary(5) variables. Things we don't use are omitted. */
    struct pkgvars {
        std::vector<pkgpattern> DEPENDS;
        pkgname PKGNAME;
        pkgpath PKGPATH;
    };

    /** summary is a map from PKGNAME to its variables, obtained by parsing
     * a pkg_summary(5) file or by scanning PACKAGES.
     */
    struct summary: public std::map<pkgname, pkgvars> {
        using std::map<pkgname, pkgvars>::map;

        summary(
            options const& opts,
            std::filesystem::path const& PACKAGES,
            std::filesystem::path const& PKG_INFO,
            std::string const& PKG_SUFX);

        summary&
        operator+= (summary&& other) {
            merge(other);
            return *this;
        }
    };

    /** pkgmap is a map from PKGPATH to a subset of summary that contains
     * only packages that correspond to that PKGPATH. The subset is further
     * grouped by their PKGBASEs. This is because some PKGPATHs (like py-*)
     * have more than a single PKGBASE, and we need to treat them as
     * separate packages.
     */
    struct pkgmap: public std::map<pkgpath, std::map<pkgbase, summary>> {
        using std::map<pkgpath, std::map<pkgbase, summary>>::map;

        pkgmap(summary const& all_packages);
    };
}
