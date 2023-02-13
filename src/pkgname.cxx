#include <algorithm>

#include "pkgname.hxx"

using namespace pkg_chk;
using namespace std::literals;

namespace {
    inline bool
    is_ascii_digit(char c) noexcept {
        return c >= '0' && c <= '9';
    }

    inline bool
    is_ascii_alpha(char c) noexcept {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    inline char
    ascii_tolower(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
    }

    template <typename Iter>
    bool
    ci_starts_with(Iter begin, Iter end, std::string_view const& str) {
        Iter it = begin;
        for (auto c: str) {
            if (it == end) {
                // The input is shorter than 'str'.
                return false;
            }
            else if (ascii_tolower(*it) == ascii_tolower(c)) {
                it++;
                continue;
            }
            else {
                // Letters mismatched.
                return false;
            }
        }
        return true;
    }

    std::vector<pkgversion::modifier> const modifiers = {
        pkgversion::modifier(pkgversion::modifier::kind::ALPHA, "alpha"),
        pkgversion::modifier(pkgversion::modifier::kind::BETA , "beta"),
        pkgversion::modifier(pkgversion::modifier::kind::RC   , "pre"),
        pkgversion::modifier(pkgversion::modifier::kind::RC   , "rc"),
        pkgversion::modifier(pkgversion::modifier::kind::DOT  , "pl"),
        pkgversion::modifier(pkgversion::modifier::kind::DOT  , "_"),
        pkgversion::modifier(pkgversion::modifier::kind::DOT  , ".")
    };
}

namespace pkg_chk {
    pkgversion::pkgversion(std::string_view const& str) {
        for (auto it = str.begin(); it != str.end(); ) {
            if (is_ascii_digit(*it)) {
                int n = 0;
                for (; it != str.end() && is_ascii_digit(*it); it++) {
                    n = n * 10 + (*it - '0');
                }
                _comps.emplace_back(digits(n));
                continue;
            }
            {
                bool found_mod = false;
                for (auto const& mod: modifiers) {
                    if (ci_starts_with(it, str.end(), mod.string())) {
                        _comps.push_back(mod);
                        it += mod.string().size();
                        found_mod = true;
                        break;
                    }
                }
                if (found_mod) {
                    continue;
                }
            }
            if (ci_starts_with(it, str.end(), "nb"sv)) {
                it += 2;
                int n = 0;
                for (; it != str.end() && is_ascii_digit(*it); it++) {
                    n = n * 10 + (*it - '0');
                }
                _comps.emplace_back(revision(n));
                continue;
            }
            if (is_ascii_alpha(*it)) {
                _comps.emplace_back(modifier(modifier::kind::DOT, ""s));
                _comps.emplace_back(alpha(*it++));
                continue;
            }
            // Dunno what to do about this character. It's an invalid
            // version. Just ignore it.
            it++;
        }
    }

    int
    pkgversion::compare(pkgversion const& other) const noexcept {
        auto const to_digit =
            [](auto const& comp) -> int {
                return comp;
            };

        for (std::vector<component>::size_type i = 0;
             i < std::max(this->_comps.size(), other._comps.size());
             i++) {

            int const a = i < this->_comps.size() ? std::visit(to_digit, this->_comps[i]) : 0;
            int const b = i < other._comps.size() ? std::visit(to_digit, other._comps[i]) : 0;
            if (a < b) {
                return -1;
            }
            else if (a > b) {
                return 1;
            }
            else {
                continue;
            }
        }
        return 0;
    }

    pkgname::pkgname(std::string_view const& name) {
        auto const hyphen = name.rfind('-');
        if (hyphen == std::string_view::npos) {
            // This shouldn't happen.
            base = pkgbase(name);
        }
        else {
            base    = pkgbase(name.substr(0, hyphen));
            version = pkgversion(name.substr(hyphen + 1));
        }
    }
}
