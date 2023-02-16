#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "pkgname.hxx"

namespace pkg_chk {
    struct pkgpattern {
        /// csh-style alternatives: foo{bar,{baz,qux}}
        struct alternatives {
            alternatives() {}
            alternatives(std::string_view const& patstr);

            using value_type     = pkgpattern;
            using iterator       = std::vector<pkgpattern>::const_iterator;
            using const_iterator = std::vector<pkgpattern>::const_iterator;

            const_iterator
            begin() const {
                return _expanded.begin();
            }

            const_iterator
            end() const {
                return _expanded.end();
            }

            operator std::string const& () const noexcept {
                return _original;
            }

            friend std::ostream&
            operator<< (std::ostream& out, alternatives const& alts);

        private:
            std::string _original;
            std::vector<pkgpattern> _expanded;
        };

        /// Version constraints: foo>=1.1<2
        struct version_range {
            /// <=
            struct le: public pkgversion {
                friend std::ostream&
                operator<< (std::ostream& out, le const& ver) {
                    return out << "<=" << static_cast<pkgversion const&>(ver);
                }
            };
            /// <
            struct lt: public pkgversion {
                friend std::ostream&
                operator<< (std::ostream& out, lt const& ver) {
                    return out << '<' << static_cast<pkgversion const&>(ver);
                }
            };
            /// >=
            struct ge {
                friend std::ostream&
                operator<< (std::ostream& out, ge const& ver);

                pkgversion min; /// >=1.1
                std::optional<
                    std::variant<
                        le,     /// >=1.1<=2
                        lt      /// >=1.1<2
                        >> sup; /// Actually a maximum if it's le.
            };
            /// >
            struct gt {
                friend std::ostream&
                operator<< (std::ostream& out, gt const& ver);

                pkgversion inf; /// >1.1
                std::optional<
                    std::variant<
                        le,     /// >1.1<=2
                        lt      /// >1.1<2
                        >> sup; /// Actually a maximum if it's le.
            };
            /// ==
            struct eq: public pkgversion {
                friend std::ostream&
                operator<< (std::ostream& out, eq const& ver) {
                    return out << "==" << static_cast<pkgversion const&>(ver);
                }
            };
            /// !=
            struct ne: public pkgversion {
                friend std::ostream&
                operator<< (std::ostream& out, ne const& ver) {
                    return out << "!=" << static_cast<pkgversion const&>(ver);
                }
            };

            version_range(std::string_view const& patstr);

            using constraint = std::variant<le, lt, ge, gt, eq, ne>;

            friend std::ostream&
            operator<< (std::ostream& out, version_range const& ver);

            pkgbase base;
            constraint cst;
        };

        /// Glob pattern: foo-[0-9]*
        struct glob: std::string {};

        using pattern_type = std::variant<
            alternatives,
            version_range,
            glob
            >;

        pkgpattern(std::string_view const& patstr);

        friend std::ostream&
        operator<< (std::ostream& out, pkgpattern const& pat);

        pattern_type pattern;
    };
}
