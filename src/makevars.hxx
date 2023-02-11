#pragma once

#include <filesystem>
#include <map>
#include <vector>

#include "options.hxx"

namespace pkg_chk {
    /** Extract a set of variables from a given mk.conf. 'vars' is a
     * sequence of variables to extract. Returns a map from variable names
     * to their value which is possibly empty.
     */
    std::map<std::string, std::string>
    extract_mkconf_vars(
        pkg_chk::options const& opts,
        std::filesystem::path const& makeconf,
        std::vector<std::string> const& vars);

    /** Extract a set of variables from a given pkgpath. 'vars' is a
     * sequence of variables to extract. Returns a map from variable names
     * to their value which is possibly empty.
     */
    std::map<std::string, std::string>
    extract_pkgmk_vars(
        pkg_chk::options const& opts,
        std::filesystem::path const& pkgpath,
        std::vector<std::string> const& vars);
}
