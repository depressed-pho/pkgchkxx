#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "environment.hxx"
#include "pkgname.hxx"
#include "todo.hxx"

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

    void
    lookup_todo(environment const& env);
}
