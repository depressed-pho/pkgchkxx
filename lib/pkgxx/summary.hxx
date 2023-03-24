#pragma once

#include <filesystem>
#include <istream>
#include <map>
#include <optional>
#include <ostream>
#include <set>

#include <pkgxx/pkgpath.hxx>
#include <pkgxx/pkgpattern.hxx>
#include <pkgxx/pkgname.hxx>

namespace pkgxx {
    /** \c pkg_summary(5) variables. Things we don't use are omitted for
     * now. */
    struct pkgvars {
        /** A set of patterns of packages the package depends on. The sole
         * reason why this isn't a \c std::set or a \c std::unordered_set
         * is that neither equality nor ordering can be meaningfully
         * defined for \ref pkgpattern.
         */
        std::vector<pkgpattern> DEPENDS;

        /** The name of the binary package file. If not given, \c
         * PKGNAME.tgz can be assumed.
         */
        std::optional<std::filesystem::path> FILE_NAME;

        /** The name of the package. */
        pkgname PKGNAME;

        /** The path of the package directory within pkgsrc. */
        pkgpath PKGPATH;
    };

    /** summary is a map from PKGNAME to its variables, obtained by parsing
     * a pkg_summary(5) file, querying pkgdb, or by scanning PACKAGES.
     */
    struct summary: public std::map<pkgname, pkgvars> {
        using std::map<pkgname, pkgvars>::map;

        /** Obtain a package summary by querying pkgdb. */
        summary(std::string const& PKG_INFO);

        /** Obtain a package summary by scanning binary packages. */
        summary(
            std::ostream& msg,
            std::ostream& verbose,
            unsigned concurrency,
            std::filesystem::path const& PACKAGES,
            std::string const& PKG_INFO,
            std::string const& PKG_SUFX);

        /// Merge two summaries into one. The summary \c other will be
        /// destroyed in the process.
        summary&
        operator+= (summary&& other) {
            merge(std::move(other));
            return *this;
        }
    };

    /** A map from PKGPATH to a subset of summary that contains only
     * packages that correspond to that PKGPATH. The subset is further
     * grouped by their PKGBASEs. This is because some PKGPATHs (like \c
     * py-*) have more than a single PKGBASE, and we need to treat them as
     * separate packages.
     */
    struct pkgmap: public std::map<pkgpath, std::map<pkgbase, summary>> {
        using std::map<pkgpath, std::map<pkgbase, summary>>::map;

        /** Construct a \ref pkgmap from a summary of all the packages in
         * interest.
         */
        pkgmap(summary const& all_packages);
    };
}
