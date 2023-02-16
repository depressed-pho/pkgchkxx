#include <string>

#include "pkgpath.hxx"

namespace pkg_chk {
    pkgpath::pkgpath(std::string_view const& path) {
        auto const slash = path.find('/');
        if (slash != std::string_view::npos && slash + 1 < path.size()) {
            category = path.substr(0, slash);
            subdir   = path.substr(slash + 1);
        }
        else {
            throw bad_pkgpath("Invalid PKGPATH: " + std::string(path));
        }
    }
}
