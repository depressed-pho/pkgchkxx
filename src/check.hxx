#pragma once

#include <map>
#include <set>

#include "options.hxx"
#include "environment.hxx"
#include "pkgname.hxx"

namespace pkg_chk {
    struct check_result {
        std::set<pkgpath>          MISSING_DONE;
        std::map<pkgname, pkgpath> MISSING_TODO;
        std::set<pkgname>          MISMATCH_TODO;
    };

    check_result
    check_installed_packages(
        options const& opts,
        environment const& env,
        std::set<pkgpath> const& pkgpaths);
}
