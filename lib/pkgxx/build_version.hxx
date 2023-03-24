#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <ostream>
#include <string>

#include <pkgxx/pkgname.hxx>
#include <pkgxx/pkgpath.hxx>

namespace pkgxx {
    /** Class that represents a build version of a package. It is a map
     * from a file path to its RCS Id string.
     */
    struct build_version: std::map<std::filesystem::path, std::string> {
        using std::map<std::filesystem::path, std::string>::map;

        /** Retrieve a build version from a binary package file, or \c
         * std::nullopt if the file does not exist.
         */
        static std::optional<build_version>
        from_binary(
            std::string const& PKG_INFO,
            std::filesystem::path const& bin_pkg_file);

        /** Retrieve a build version from an installed package, or \c
         * std::nullopt if the package isn't installed.
         */
        static std::optional<build_version>
        from_installed(
            std::string const& PKG_INFO,
            pkgname const& name);

        /** Retrieve a build version from source, or \c std::nullopt if the
         * package path doesn't exist.
         */
        static std::optional<build_version>
        from_source(
            std::filesystem::path const& PKGSRCDIR,
            pkgpath const& path);

        /** Print a build version to an output stream. */
        friend std::ostream&
        operator<< (std::ostream& out, build_version const& bv);
    };
}
