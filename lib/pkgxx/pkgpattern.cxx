#include <cassert>
#include <exception>
#include <optional>
#include <variant>

#include "pkgpattern.hxx"
#include "string_algo.hxx"

using namespace std::literals;

namespace pkgxx {
    bad_pkgpattern::bad_pkgpattern(std::string_view const& pattern)
        : std::runtime_error("")
        , pattern{pattern} {}

    char const*
    bad_pkgpattern::what() const noexcept {
        if (!msg) {
            msg.emplace("Malformed pattern: " + pattern);
        }
        return msg->c_str();
    }

    bad_alternatives::bad_alternatives(std::string_view const& alternatives)
        : std::runtime_error("")
        , alternatives{alternatives} {}

    char const*
    bad_alternatives::what() const noexcept {
        if (!msg) {
            msg.emplace("Malformed alternatives: " + alternatives);
        }
        return msg->c_str();
    }

    bad_version_range::bad_version_range(std::string_view const& version_range)
        : std::runtime_error("")
        , version_range{version_range} {}

    char const*
    bad_version_range::what() const noexcept {
        if (!msg) {
            msg.emplace("Malformed version range: " + version_range);
        }
        return msg->c_str();
    }

    pkgpattern::alternatives::alternatives(std::string_view const& patstr)
        : _original(patstr) {

        // Extract the part preceding the opening brace.
        auto const o_brace = patstr.find('{');
        assert(o_brace != std::string_view::npos);
        auto const head = patstr.substr(0, o_brace);

        // Extract the part following the closing brace. Note that it may
        // contain other sets of braces, which will be handled by recursive
        // calls of parse_alternatives().
        auto c_brace = std::string_view::npos;
        {
            int level = 0;
            for (auto i = o_brace; i < patstr.size(); i++) {
                if (patstr[i] == '{') {
                    level++;
                }
                else if (patstr[i] == '}') {
                    level--;
                    if (level == 0) {
                        c_brace = i;
                        break;
                    }
                }
            }
            if (level != 0) {
                throw bad_alternatives{patstr};
            }
        }
        auto const tail = c_brace + 1 < patstr.size()
            ? patstr.substr(c_brace + 1)
            : ""sv;

        // Now iterate comma-separated alternatives enclosed by outermost
        // braces.
        for (auto seg_begin = o_brace + 1;; ) {
            auto seg_end = std::string_view::npos;
            int level = 1;
            for (auto i = seg_begin;; i++) {
                if (patstr[i] == '{') {
                    level++;
                }
                else if (patstr[i] == ',') {
                    if (level == 1) {
                        seg_end = i;
                        break;
                    }
                }
                else if (patstr[i] == '}') {
                    level--;
                    if (level == 0) {
                        seg_end = i;
                        break;
                    }
                }
            }
            assert(seg_end != std::string_view::npos);
            auto const segment = patstr.substr(seg_begin, seg_end - seg_begin);

            try {
                _expanded.emplace_back(
                    std::string(head) + std::string(segment) + std::string(tail));
            }
            catch (...) {
                std::throw_with_nested(bad_alternatives{patstr});
            }

            if (patstr[seg_end] == '}') {
                break;
            }
            else {
                // Given that we haven't reached the closing brace yet,
                // seg_end+1 is guaranteed to be within the valid range of
                // indices.
                seg_begin = seg_end + 1;
            }
        }
    }

    std::ostream&
    operator<< (std::ostream& out, pkgpattern::alternatives const& alts) {
        return out << alts._original;
    }

    std::ostream&
    operator<< (std::ostream& out, pkgpattern::version_range::ge const& ver) {
        out << ">=" << ver.min;
        if (ver.sup) {
            std::visit(
                [&](auto const& sup) {
                    out << sup;
                },
                *(ver.sup));
        }
        return out;
    }

    std::ostream&
    operator<< (std::ostream& out, pkgpattern::version_range::gt const& ver) {
        out << '>' << ver.inf;
        if (ver.sup) {
            std::visit(
                [&](auto const& sup) {
                    out << sup;
                },
                *(ver.sup));
        }
        return out;
    }

    pkgpattern::version_range::version_range(std::string_view const& patstr) {
        auto const op_begin = patstr.find_first_of("<>!=");
        assert(op_begin != std::string_view::npos);
        base = pkgbase(patstr.substr(0, op_begin));

        if (patstr[op_begin] == '<') { // <= or <
            if (starts_with(patstr.substr(op_begin), "<="sv)) {
                auto const op_end = op_begin + 2;
                if (op_end < patstr.size()) {
                    try {
                        cst = pkgpattern::version_range::le {
                            pkgversion(patstr.substr(op_end))
                        };
                    }
                    catch (...) {
                        std::throw_with_nested(bad_version_range{patstr});
                    }
                }
                else {
                    throw bad_version_range{patstr};
                }
            }
            else {
                auto const op_end = op_begin + 1;
                if (op_end < patstr.size()) {
                    try {
                        cst = pkgpattern::version_range::lt {
                            pkgversion(patstr.substr(op_end))
                        };
                    }
                    catch (...) {
                        std::throw_with_nested(bad_version_range{patstr});
                    }
                }
                else {
                    throw bad_version_range{patstr};
                }
            }
        }
        else if (patstr[op_begin] == '>') { // >= or >
            auto const is_ge  = starts_with(patstr.substr(op_begin), ">="sv);
            auto const op_end = op_begin + (is_ge ? 2 : 1);

            pkgversion inf;
            std::optional<
                std::variant<
                    pkgpattern::version_range::le,
                    pkgpattern::version_range::lt>> sup;
            if (auto const op2_begin = patstr.find('<', op_end);
                op2_begin != std::string_view::npos) {

                try {
                    inf = pkgversion(patstr.substr(op_end, op2_begin - op_end));
                }
                catch (...) {
                    std::throw_with_nested(bad_version_range{patstr});
                }

                if (starts_with(patstr.substr(op2_begin), "<=")) {
                    auto const op2_end = op2_begin + 2;
                    if (op2_end < patstr.size()) {
                        try {
                            sup = pkgpattern::version_range::le {
                                pkgversion(patstr.substr(op2_end))
                            };
                        }
                        catch (...) {
                            std::throw_with_nested(bad_version_range{patstr});
                        }
                    }
                    else {
                        throw bad_version_range{patstr};
                    }
                }
                else {
                    auto const op2_end = op2_begin + 1;
                    if (op2_end < patstr.size()) {
                        try {
                            sup = pkgpattern::version_range::lt {
                                pkgversion(patstr.substr(op2_end))
                            };
                        }
                        catch (...) {
                            std::throw_with_nested(bad_version_range{patstr});
                        }
                    }
                    else {
                        throw bad_version_range{patstr};
                    }
                }
            }
            else {
                try {
                    inf = pkgversion(patstr.substr(op_end));
                }
                catch (...) {
                    std::throw_with_nested(bad_version_range{patstr});
                }
            }

            if (is_ge) {
                cst = pkgpattern::version_range::ge {
                    std::move(inf),
                    std::move(sup)
                };
            }
            else {
                cst = pkgpattern::version_range::gt {
                    std::move(inf),
                    std::move(sup)
                };
            }
        }
        else if (starts_with(patstr.substr(op_begin), "==")) {
            if (auto const op_end = op_begin + 2; op_end < patstr.size()) {
                try {
                    cst = pkgpattern::version_range::eq {
                        pkgversion(patstr.substr(op_end))
                    };
                }
                catch (...) {
                    std::throw_with_nested(bad_version_range{patstr});
                }
            }
            else {
                throw bad_version_range{patstr};
            }
        }
        else if (starts_with(patstr.substr(op_begin), "!=")) {
            if (auto const op_end = op_begin + 2; op_end < patstr.size()) {
                try {
                    cst = pkgpattern::version_range::ne {
                        pkgversion(patstr.substr(op_end))
                    };
                }
                catch (...) {
                    std::throw_with_nested(bad_version_range{patstr});
                }
            }
            else {
                throw bad_version_range{patstr};
            }
        }
        else {
            throw bad_version_range{patstr};
        }
    }

    std::ostream&
    operator<< (std::ostream& out, pkgpattern::version_range const& ver) {
        out << ver.base;
        std::visit(
            [&](auto const& cst) {
                out << cst;
            },
            ver.cst);
        return out;
    }

    pkgpattern::pkgpattern(std::string_view const& patstr) {
        try {
            if (patstr.find('{') != std::string_view::npos) {
                _pat = alternatives(patstr);
            }
            else if (patstr.find_first_of("<>!=") != std::string_view::npos) {
                _pat = version_range(patstr);
            }
            else {
                _pat = glob { std::string(patstr) };
            }
        }
        catch (...) {
            std::throw_with_nested(bad_pkgpattern{patstr});
        }
    }

    std::ostream&
    operator<< (std::ostream& out, pkgpattern const& pat) {
        std::visit(
            [&](auto const& p) {
                out << p;
            },
            static_cast<pkgpattern::pattern_type const&>(pat));
        return out;
    }
}
