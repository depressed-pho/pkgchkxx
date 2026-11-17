#include <string>

#include "pkgpath.hxx"

namespace pkgxx {
    bad_pkgpath::bad_pkgpath(std::string_view const& pkgpath)
        : std::runtime_error("")
        , pkgpath(pkgpath) {}

    char const*
    bad_pkgpath::what() const noexcept {
        if (!msg) {
            msg.emplace("`" + pkgpath + "' is not a valid PKGPATH");
        }
        return msg->c_str();
    }

    pkgpath::pkgpath(std::string_view const& path) {
        auto const slash = path.find('/');
        if (slash == std::string_view::npos) {
            throw bad_pkgpath(path);
        }

        category = path.substr(0, slash);
        subdir   = path.substr(slash + 1);

        if (category.length() == 0 || subdir.length() == 0) {
            throw bad_pkgpath(path);
        }
    }
}
