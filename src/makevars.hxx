#pragma once

#include <map>
#include <vector>

#include "options.hxx"

namespace pkg_chk {
    /* Extract a set of variables from a given mk.conf. 'vars' is a
     * sequence of variables to extract. Returns a map from variable names
     * to their value, and the value is possibly empty.
     */
    std::map<std::string, std::string>
    extract_mk_vars(
        pkg_chk::options const& opts,
        std::string const& makeconf,
        std::vector<std::string> const& vars);
}
