#pragma once

#include <exception>
#include <filesystem>
#include <ostream>
#include <string_view>

#include <pkgxx/hash.hxx>
#include <pkgxx/ordered.hxx>

namespace pkgxx {
    struct bad_pkgpath: std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    /** A class that represents a PKGPATH; a pair of package category and a
     * sub-directory.
     */
    struct pkgpath {
        pkgpath() = delete;

        /** Parse a PKGPATH string. */
        pkgpath(std::string_view const& dir);

        /** Convert a PKGPATH into a relative \c path object. */
        operator std::filesystem::path () const {
            return std::filesystem::path(category) / subdir;
        }

        /// \ref pkgpath equality.
        friend bool
        operator== (pkgpath const& a, pkgpath const& b) noexcept {
            return
                a.category == b.category &&
                a.subdir   == b.subdir;
        }

        /// \ref pkgpath ordering.
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

        /// Print the string representation of PKGPATH to an output stream.
        friend std::ostream&
        operator<< (std::ostream& out, pkgpath const& dir) {
            out << dir.category << '/' << dir.subdir;
            return out;
        }

        std::string category; ///< Package category
        std::string subdir;   ///< Package subdirectory
    };
}

template <>
struct std::hash<pkgxx::pkgpath> {
    std::size_t
    operator() (pkgxx::pkgpath const& path) const noexcept {
        return pkgxx::hash_combine(path.category, path.subdir);
    }
};
