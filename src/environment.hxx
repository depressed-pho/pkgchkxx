#pragma once

#include <string>

#include "options.hxx"

namespace pkg_chk {
    struct environment {
        environment(pkg_chk::options const& opts);

        std::string PKG_PATH;
        std::string MAKECONF;
        std::string PKGSRCDIR;
        std::string LOCALBASE;
    };
}
