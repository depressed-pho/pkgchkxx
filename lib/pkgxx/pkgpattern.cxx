#include <cassert>
#include <exception>
#include <optional>
#include <variant>

#include "pkgpattern.hxx"
#include "string_algo.hxx"

using namespace std::literals;

namespace pkgxx {
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
                throw std::runtime_error("Malformed alternate `" + std::string(patstr) + "'");
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

            _expanded.emplace_back(
                std::string(head) + std::string(segment) + std::string(tail));

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
                    cst = pkgpattern::version_range::le {
                        pkgversion(patstr.substr(op_end))
                    };
                }
                else {
                    throw std::runtime_error(
                        "Malformed version constraint `" + std::string(patstr) + "'");
                }
            }
            else {
                auto const op_end = op_begin + 1;
                if (op_end < patstr.size()) {
                    cst = pkgpattern::version_range::lt {
                        pkgversion(patstr.substr(op_end))
                    };
                }
                else {
                    throw std::runtime_error(
                        "Malformed version constraint `" + std::string(patstr) + "'");
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

                inf = pkgversion(patstr.substr(op_end, op2_begin - op_end));
                if (starts_with(patstr.substr(op2_begin), "<=")) {
                    auto const op2_end = op2_begin + 2;
                    if (op2_end < patstr.size()) {
                        sup = pkgpattern::version_range::le {
                            pkgversion(patstr.substr(op2_end))
                        };
                    }
                    else {
                        throw std::runtime_error(
                            "Malformed version constraint `" + std::string(patstr) + "'");
                    }
                }
                else {
                    auto const op2_end = op2_begin + 1;
                    if (op2_end < patstr.size()) {
                        sup = pkgpattern::version_range::lt {
                            pkgversion(patstr.substr(op2_end))
                        };
                    }
                    else {
                        throw std::runtime_error(
                            "Malformed version constraint `" + std::string(patstr) + "'");
                    }
                }
            }
            else {
                inf = pkgversion(patstr.substr(op_end));
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
                cst = pkgpattern::version_range::eq {
                    pkgversion(patstr.substr(op_end))
                };
            }
            else {
                throw std::runtime_error(
                    "Malformed version constraint `" + std::string(patstr) + "'");
            }
        }
        else if (starts_with(patstr.substr(op_begin), "!=")) {
            if (auto const op_end = op_begin + 2; op_end < patstr.size()) {
                cst = pkgpattern::version_range::ne {
                    pkgversion(patstr.substr(op_end))
                };
            }
            else {
                throw std::runtime_error(
                    "Malformed version constraint `" + std::string(patstr) + "'");
            }
        }
        else {
            throw std::runtime_error(
                "Malformed version constraint `" + std::string(patstr) + "'");
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
