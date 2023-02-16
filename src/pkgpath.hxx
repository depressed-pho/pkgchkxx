#pragma once

#include <exception>
#include <filesystem>
#include <ostream>
#include <string_view>

#include "ordered.hxx"

namespace pkg_chk {
    struct bad_pkgpath: std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct pkgpath {
        pkgpath() {}
        pkgpath(std::string_view const& dir);

        friend bool
        operator== (pkgpath const& a, pkgpath const& b) noexcept {
            return
                a.category == b.category &&
                a.subdir   == b.subdir;
        }

        friend bool
        operator< (pkgpath const& a, pkgpath const& b) noexcept {
            if (a.category < b.category) {
                return true;
            }
            else {
                return
                    a.category == b.category &&
                    a.subdir   <  b.subdir;
            }
        }

        friend std::ostream&
        operator<< (std::ostream& out, pkgpath const& dir) {
            out << dir.category << '/' << dir.subdir;
            return out;
        }

        std::string category;
        std::string subdir;
    };
}
