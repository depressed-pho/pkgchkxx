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

#include "ordered.hxx"

namespace pkg_chk {
    using pkgbase = std::string;

    struct pkgversion: ordered<pkgversion> {
        struct digits {
            digits(int num, int width = -1) noexcept
                : _num(num)
                , _width(width) {}

            operator int() const noexcept {
                return _num;
            }

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

        struct modifier {
            enum class kind: int {
                ALPHA = -3,
                BETA  = -2,
                RC    = -1,
                DOT   = 0
            };

            modifier(kind k, std::string const& str) noexcept
                : _kind(k)
                , _str(str) {}

            modifier(kind k, std::string&& str) noexcept
                : _kind(k)
                , _str(str) {}

            operator int() const noexcept {
                return static_cast<int>(_kind);
            }

            operator std::string const&() const noexcept {
                return string();
            }

            std::string const&
            string() const noexcept {
                return _str;
            }

            friend std::ostream&
            operator<< (std::ostream& out, pkgversion::modifier const& mod) {
                return out << mod._str;
            }

        private:
            kind _kind;
            std::string _str;
        };

        /// "nb" suffix
        struct revision {
            revision(int rev) noexcept
                : _rev(rev) {}

            operator int() const noexcept {
                return _rev;
            }

            friend std::ostream&
            operator<< (std::ostream& out, pkgversion::revision const& rev) {
                return out << "nb" << rev._rev;
            }

        private:
            int _rev;
        };

        struct alpha {
            alpha(char c) noexcept
                : _c(c) {

                assert((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
            }

            operator char() const noexcept {
                return _c;
            }

            operator int() const noexcept {
                return _c >= 'a' ? _c - 'a' + 1 : _c - 'A' + 1;
            }

            friend std::ostream&
            operator<< (std::ostream& out, pkgversion::alpha const& alpha) {
                return out << alpha._c;
            }

        private:
            char _c;
        };

        using component = std::variant<
            digits,
            modifier,
            revision,
            alpha
            >;

        using iterator       = std::vector<component>::iterator;
        using const_iterator = std::vector<component>::const_iterator;

        /** Construct an empty pkgversion object representing negative
         * infinity with respect to ordering.
         */
        pkgversion() {}

        pkgversion(std::string const& str)
            : pkgversion(static_cast<std::string_view>(str)) {}

        pkgversion(std::string_view const& str);

        iterator
        begin() {
            return _comps.begin();
        }

        const_iterator
        begin() const {
            return _comps.begin();
        }

        iterator
        end() {
            return _comps.end();
        }

        const_iterator
        end() const {
            return _comps.end();
        }

        bool
        operator== (pkgversion const& other) const noexcept {
            return compare(other) == 0;
        }

        bool
        operator< (pkgversion const& other) const noexcept {
            return compare(other) < 0;
        }

        friend std::ostream&
        operator<< (std::ostream& out, pkgversion::component const& comp) {
            std::visit(
                [&out](auto const& c) {
                    out << c;
                },
                comp);
            return out;
        }

        friend std::ostream&
        operator<< (std::ostream& out, pkgversion const& version) {
            for (auto const& comp: version) {
                out << comp;
            }
            return out;
        }

    private:
        int
        compare(pkgversion const& other) const noexcept;

        std::vector<component> _comps;
    };

    struct pkgname: ordered<pkgname> {
        pkgname(std::string_view const& name);

        template <typename Base, typename Version>
        pkgname(Base&& base_, Version&& version_)
            : base(std::forward<Base>(base_))
            , version(std::forward<Version>(version_)) {}

        std::string
        string() const {
            std::stringstream ss;
            ss << *this;
            return ss.str();
        }

        friend bool
        operator== (pkgname const& a, pkgname const& b) noexcept {
            return
                a.base    == b.base     &&
                a.version == b.version;
        }

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

        friend std::ostream&
        operator<< (std::ostream& out, pkgname const& name) {
            return out << name.base << "-" << name.version;
        }

        pkgbase    base;
        pkgversion version;
    };
}
