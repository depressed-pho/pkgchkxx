#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <ostream>
#include <string>

#include "pkgname.hxx"
#include "pkgpath.hxx"

namespace pkg_chk {
    struct build_version: std::map<std::filesystem::path, std::string> {
        using std::map<std::filesystem::path, std::string>::map;

        static std::optional<build_version>
        from_binary(
            std::string const& PKG_INFO,
            std::filesystem::path const& bin_pkg_file);

        static std::optional<build_version>
        from_installed(
            std::string const& PKG_INFO,
            pkgname const& name);

        static std::optional<build_version>
        from_source(
            std::filesystem::path const& PKGSRCDIR,
            pkgpath const& path);

        friend std::ostream&
        operator<< (std::ostream& out, build_version const& bv);
    };
}
