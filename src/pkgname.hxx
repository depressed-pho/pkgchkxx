#pragma once

#include <cassert>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace pkg_chk {
    using pkgbase = std::string;

    struct pkgversion {
        struct digits {
            digits(int num) noexcept
                : _num(num) {};

            operator int() const noexcept {
                return _num;
            }

        private:
            int _num;
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
        operator!= (pkgversion const& other) const noexcept {
            return compare(other) != 0;
        }

        bool
        operator<= (pkgversion const& other) const noexcept {
            return compare(other) <= 0;
        }

        bool
        operator< (pkgversion const& other) const noexcept {
            return compare(other) < 0;
        }

        bool
        operator>= (pkgversion const& other) const noexcept {
            return compare(other) >= 0;
        }

        bool
        operator> (pkgversion const& other) const noexcept {
            return compare(other) > 0;
        }

    private:
        int
        compare(pkgversion const& other) const noexcept;

        std::vector<component> _comps;
    };

    inline std::ostream&
    operator<< (std::ostream& out, pkgversion::digits const& digits) {
        return out << static_cast<int>(digits);
    }

    inline std::ostream&
    operator<< (std::ostream& out, pkgversion::modifier const& mod) {
        return out << mod.string();
    }

    inline std::ostream&
    operator<< (std::ostream& out, pkgversion::revision const& rev) {
        return out << "nb" << static_cast<int>(rev);
    }

    inline std::ostream&
    operator<< (std::ostream& out, pkgversion::alpha const& alpha) {
        return out << static_cast<char>(alpha);
    }

    inline std::ostream&
    operator<< (std::ostream& out, pkgversion::component const& comp) {
        std::visit(
            [&out](auto const& c) {
                out << c;
            },
            comp);
        return out;
    }

    inline std::ostream&
    operator<< (std::ostream& out, pkgversion const& version) {
        for (auto const& comp: version) {
            out << comp;
        }
        return out;
    }

    struct pkgname {
        pkgname(std::string_view const& name);

        template <typename Base, typename Version>
        pkgname(Base&& base_, Version&& version_)
            : base(std::forward<Base>(base_))
            , version(std::forward<Version>(version_)) {}

        pkgbase    base;
        pkgversion version;
    };

    inline std::ostream&
    operator<< (std::ostream& out, pkgname const& name) {
        return out << name.base << "-" << name.version;
    }
}
