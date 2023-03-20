#pragma once

#include <map>
#include <set>

#include <pkgxx/pkgname.hxx>

#include "options.hxx"
#include "environment.hxx"

namespace pkg_chk {
    struct check_result {
        std::set<pkgxx::pkgpath>                 MISSING_DONE;
        std::map<pkgxx::pkgname, pkgxx::pkgpath> MISSING_TODO;
        std::set<pkgxx::pkgname>                 MISMATCH_TODO;
    };

    check_result
    check_installed_packages(
        options const& opts,
        environment const& env,
        std::set<pkgxx::pkgpath> const& pkgpaths);
}
