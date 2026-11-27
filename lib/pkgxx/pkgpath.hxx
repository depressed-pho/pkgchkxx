#pragma once

#include <cereal/cereal.hpp>
#include <exception>
#include <filesystem>
#include <optional>
#include <ostream>
#include <pkgxx/hash.hxx>
#include <pkgxx/ordered.hxx>
#include <string_view>

namespace pkgxx {
    struct bad_pkgpath: public std::runtime_error {
        bad_pkgpath(std::string_view const& pkgpath);

        virtual char const*
        what() const noexcept override;

        std::string pkgpath;

    private:
        mutable std::optional<std::string> msg;
    };

    /** A type alias that represents a package category. */
    using category = std::string;

    /** A class that represents a PKGPATH; a pair of package category and a
     * sub-directory.
     */
    struct pkgpath {
        /** Constructing an empty PKGPATH is not disallowed but is
         * discouraged, because it's not a valid PKGPATH.
         */
        pkgpath() = default;

        /** Parse a PKGPATH string. */
        pkgpath(std::string_view const& dir);

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

        /** Convert a PKGPATH into a string. */
        std::string
        string() const {
            return static_cast<std::filesystem::path>(*this).string();
        }

        /** Convert a PKGPATH into a relative \c path object. */
        operator std::filesystem::path () const {
            return std::filesystem::path(category) / subdir;
        }

        /** Serialise the PKGPATH. */
        template <typename Archive>
        std::string
        save_minimal(Archive&) const {
            return string();
        }

        /** Deserialise the PKGPATH. */
        template <typename Archive>
        void
        load_minimal(Archive const&, std::string const& path) {
            *this = pkgpath{path};
        }

        /** Print the string representation of PKGPATH to an output
         * stream.
         */
        friend std::ostream&
        operator<< (std::ostream& out, pkgpath const& path) {
            out << path.category << '/' << path.subdir;
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
