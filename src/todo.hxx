#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "pkgname.hxx"

namespace pkg_chk {
    struct todo_entry {
        pkgname     name;
        std::string comment;
    };

    struct todo_file: public std::map<pkgbase, todo_entry> {
        using std::map<pkgbase, todo_entry>::map;

        /** Read the pkgsrc TODO file and collect "o PKGNAME" lines. */
        todo_file(std::filesystem::path const& file);
    };
}
