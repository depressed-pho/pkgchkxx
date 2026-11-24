#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <pkgxx/pkgname.hxx>

namespace pkgxx {
    /** A struct representing an entry in the pkgsrc TODO file. */
    struct todo_entry {
        pkgname     name;    ///< A package name that is requested for updating.
        std::string comment; ///< A possibly empty comment about the entry.
    };

    /** A class representing entries in the pkgsrc TODO file. */
    struct todo_file: public std::unordered_map<pkgbase, todo_entry> {
        using std::unordered_map<pkgbase, todo_entry>::unordered_map;

        /** Read the pkgsrc TODO file and collect "o PKGNAME" lines. */
        todo_file(std::filesystem::path const& file);
    };
}
