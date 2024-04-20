#include <algorithm>

#include "pkgname.hxx"
#include "string_algo.hxx"

using namespace std::literals;

namespace {
    using namespace pkgxx;

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

namespace pkgxx {
    pkgversion::pkgversion(std::string_view const& str)
        : _rev(0) {

        for (auto it = str.begin(); it != str.end(); ) {
            if (is_ascii_digit(*it)) {
                int n = 0;
                int w = 0;
                for (; it != str.end() && is_ascii_digit(*it); it++) {
                    n = n * 10 + (*it - '0');
                    w++;
                }
                _comps.emplace_back(digits(n, w));
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
            if (ci_starts_with(it, str.end(), "nb"sv) &&
                std::all_of(it + 2, str.end(), is_ascii_digit)) {

                it += 2;
                for (; it != str.end() && is_ascii_digit(*it); it++) {
                    _rev = _rev * 10 + static_cast<unsigned>(*it - '0');
                }
                break;
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
        if (is_neg_inf()) {
            return other.is_neg_inf() ? 0 : -1;
        }
        else if (other.is_neg_inf()) {
            return 1;
        }
        else {
            auto const to_digit =
                [](auto const& comp) -> int {
                    return comp;
                };

            for (std::vector<component>::size_type i = 0;
                 i < std::max(this->_comps.size(), other._comps.size());
                 i++) {

                int const a    = i < this->_comps.size() ? std::visit(to_digit, this->_comps[i]) : 0;
                int const b    = i < other._comps.size() ? std::visit(to_digit, other._comps[i]) : 0;
                int const diff = a - b;

                if (diff != 0) {
                    return diff;
                }
            }

            return static_cast<int>(_rev) - static_cast<int>(other._rev);
        }
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
