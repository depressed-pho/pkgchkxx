#pragma once

#include <cassert>
#include <iomanip>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <pkgxx/ordered.hxx>

namespace pkgxx {
    /** A type alias that represents a PKGBASE. */
    using pkgbase = std::string;

    /** A class that represents a package version. */
    struct pkgversion: ordered<pkgversion> {
        /** Digits occuring in a package version. */
        struct digits {
            /** Construct an instance of \ref digits with the number it
             * should represent and its intended width.
             *
             * \b Example:
             * \code
             * digits(1, 3); // yields an instance representing "001".
             * digits(21);   // yields an instance representing "12".
             * \endcode
             */
            digits(int num, int width = -1) noexcept
                : _num(num)
                , _width(width) {}

            /// Turn an instance of \ref digits into the number it
            /// represents.
            operator int() const noexcept {
                return _num;
            }

            /// Print an instance of \ref digits to an output stream.
            friend std::ostream&
            operator<< (std::ostream& out, pkgversion::digits const& digits) {
                if (digits._width >= 0) {
                    out << std::setfill('0') << std::setw(digits._width);
                }
                return out << digits._num;
            }

        private:
            int _num;
            int _width;
        };

        /** A modifier is a specially-treated string occuring in a package version. */
        struct modifier {
            /// The kind of modifier.
            enum class kind: int {
                ALPHA = -3,
                BETA  = -2,
                RC    = -1,
                DOT   = 0
            };

            /// Construct an instance of \ref modifier with its kind and
            /// its original string.
            modifier(kind k, std::string const& str) noexcept
                : _kind(k)
                , _str(str) {}

            /// Construct an instance of \ref modifier with its kind and
            /// its original string.
            modifier(kind k, std::string&& str) noexcept
                : _kind(k)
                , _str(str) {}

            /// Turn a modifier into an integer representing its kind.
            operator int() const noexcept {
                return static_cast<int>(_kind);
            }

            /// Obtain the original string of a modifier.
            operator std::string const&() const noexcept {
                return string();
            }

            /// Obtain the original string of a modifier.
            std::string const&
            string() const noexcept {
                return _str;
            }

            /// Print a modifier to an output stream.
            friend std::ostream&
            operator<< (std::ostream& out, pkgversion::modifier const& mod) {
                return out << mod._str;
            }

        private:
            kind _kind;
            std::string _str;
        };

        /** A latin alphabet occuring in a package version. */
        struct alpha {
            /// Turn a latin alphabet into an instance of \ref alpha.
            alpha(char c) noexcept
                : _c(c) {

                assert((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
            }

            /// Turn an instance of \ref alpha into a latin alphabet.
            operator char() const noexcept {
                return _c;
            }

            /// Convert an instance of \ref alpha into an integer that
            /// corresponds to the alphabet.
            operator int() const noexcept {
                return _c >= 'a' ? _c - 'a' + 1 : _c - 'A' + 1;
            }

            /// Print the alphabet to an output stream.
            friend std::ostream&
            operator<< (std::ostream& out, pkgversion::alpha const& alpha) {
                return out << alpha._c;
            }

        private:
            char _c;
        };

        /** Possible variants of a package version component. */
        using component = std::variant<
            digits,
            modifier,
            alpha
            >;

        /** Construct an empty \ref pkgversion object representing negative
         * infinity with respect to ordering.
         */
        pkgversion()
            : _rev(0) {}

        /** Parse a PKGVERSION string. */
        pkgversion(std::string const& str)
            : pkgversion(static_cast<std::string_view>(str)) {}

        /** Parse a PKGVERSION string. */
        pkgversion(std::string_view const& str);

        /** \ref pkgversion equality. */
        bool
        operator== (pkgversion const& other) const noexcept {
            return compare(other) == 0;
        }

        /** \ref pkgversion ordering. */
        bool
        operator< (pkgversion const& other) const noexcept {
            return compare(other) < 0;
        }

        /** Print the string representation of \ref pkgversion::component
         * to an output stream.
         */
        friend std::ostream&
        operator<< (std::ostream& out, pkgversion::component const& comp) {
            std::visit(
                [&out](auto const& c) {
                    out << c;
                },
                comp);
            return out;
        }

        /** Print the string representation of \ref pkgversion to an output
         * stream.
         */
        friend std::ostream&
        operator<< (std::ostream& out, pkgversion const& version) {
            for (auto const& comp: version._comps) {
                out << comp;
            }
            if (version._rev > 0) {
                out << "nb" << version._rev;
            }
            return out;
        }

    private:
        bool
        is_neg_inf() const noexcept {
            return _comps.empty();
        }

        int
        compare(pkgversion const& other) const noexcept;

        std::vector<component> _comps;
        unsigned _rev; // "nb" suffix
    };

    /** A class representing a PKGNAME; a pair of a PKGBASE and a
     * PKGVERSION.
     */
    struct pkgname: ordered<pkgname> {
        /** Parse a PKGNAME string e.g. <tt>foo-1.0</tt>. */
        pkgname(std::string_view const& name);

        /** Forward-construct an instance of \ref pkgname. */
        template <typename Base, typename Version>
        pkgname(Base&& base_, Version&& version_)
            : base(std::forward<Base>(base_))
            , version(std::forward<Version>(version_)) {}

        /** Obtain the string representation of \ref pkgname. */
        std::string
        string() const {
            std::stringstream ss;
            ss << *this;
            return ss.str();
        }

        /// \ref pkgname equality.
        friend bool
        operator== (pkgname const& a, pkgname const& b) noexcept {
            return
                a.base    == b.base     &&
                a.version == b.version;
        }

        /// \ref pkgname ordering.
        friend bool
        operator< (pkgname const& a, pkgname const& b) noexcept {
            if (a.base < b.base) {
                return true;
            }
            else {
                return
                    a.base    == b.base     &&
                    a.version  < b.version;
            }
        }

        /// Print the string representation to an output stream.
        friend std::ostream&
        operator<< (std::ostream& out, pkgname const& name) {
            return out << name.base << "-" << name.version;
        }

        pkgbase    base;    ///< The PKGBASE.
        pkgversion version; ///< The PKGVERSION.
    };
}
