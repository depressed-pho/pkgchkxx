#pragma once

#include <fnmatch.h>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <pkgxx/pkgname.hxx>
#include <pkgxx/string_algo.hxx>

namespace pkgxx {
    namespace detail {
        template <typename Iterator>
        inline constexpr auto const&
        pkgname_at(Iterator&& it) {
            if constexpr (std::is_same_v<pkgname, std::decay_t<decltype(*it)>>) {
                return *it;
            }
            else {
                return it->first;
            }
        }
    }

    /** A class that represents a package name pattern.
     */
    struct pkgpattern {
        /// csh-style alternatives, e.g. \c foo{bar,{baz,qux}}
        class alternatives {
        public:
            alternatives() {}

            /// Parse a string representation of alternatives.
            alternatives(std::string_view const& patstr);

            /// The const iterator that iterates through alternative
            /// patterns.
            using const_iterator = std::vector<pkgpattern>::const_iterator;

            /// Return an iterator to the beginning of alternatives.
            const_iterator
            begin() const {
                return _expanded.begin();
            }

            /// Return an iterator to the end of alternatives.
            const_iterator
            end() const {
                return _expanded.end();
            }

            /// Obtain a string representation of alternatives.
            operator std::string const& () const noexcept {
                return _original;
            }

            /// \sa pkgpattern::for_each
            template <typename Set, typename Function>
            void
            for_each(Set&& s, Function&& f) const;

            /// Print a string representation of alternatives to an output
            /// stream.
            friend std::ostream&
            operator<< (std::ostream& out, alternatives const& alts);

        private:
            std::string _original;
            std::vector<pkgpattern> _expanded;
        };

        /// Version constraints, e.g. \c foo>=1.1<2
        struct version_range {
            /// \c <=
            struct le: public pkgversion {
                /// \sa pkgpattern::for_each
                template <typename Set, typename Function>
                void
                for_each(Set&& s, Function&& f, pkgbase const& base) const;

                /// Print a version constraint to an output stream.
                friend std::ostream&
                operator<< (std::ostream& out, le const& ver) {
                    return out << "<=" << static_cast<pkgversion const&>(ver);
                }
            };
            /// \c <
            struct lt: public pkgversion {
                /// \sa pkgpattern::for_each
                template <typename Set, typename Function>
                void
                for_each(Set&& s, Function&& f, pkgbase const& base) const;

                /// Print a version constraint to an output stream.
                friend std::ostream&
                operator<< (std::ostream& out, lt const& ver) {
                    return out << '<' << static_cast<pkgversion const&>(ver);
                }
            };
            /// \c >=
            struct ge {
                /// \sa pkgpattern::for_each
                template <typename Set, typename Function>
                void
                for_each(Set&& s, Function&& f, pkgbase const& base) const;

                /// Print a version constraint to an output stream.
                friend std::ostream&
                operator<< (std::ostream& out, ge const& ver);

                pkgversion min; ///< \c >=1.1
                std::optional<
                    std::variant<
                        le,     // >=1.1<=2
                        lt      // >=1.1<2
                        >> sup; ///< Actually a maximum if it's \ref le.
            };
            /// \c >
            struct gt {
                /// \sa pkgpattern::for_each
                template <typename Set, typename Function>
                void
                for_each(Set&& s, Function&& f, pkgbase const& base) const;

                /// Print a version constraint to an output stream.
                friend std::ostream&
                operator<< (std::ostream& out, gt const& ver);

                pkgversion inf; ///< \c >1.1
                std::optional<
                    std::variant<
                        le,     // >1.1<=2
                        lt      // >1.1<2
                        >> sup; ///< Actually a maximum if it's \ref le.
            };
            /// \c ==
            struct eq: public pkgversion {
                /// \sa pkgpattern::for_each
                template <typename Set, typename Function>
                void
                for_each(Set&& s, Function&& f, pkgbase const& base) const;

                /// Print a version constraint to an output stream.
                friend std::ostream&
                operator<< (std::ostream& out, eq const& ver) {
                    return out << "==" << static_cast<pkgversion const&>(ver);
                }
            };
            /// \c !=
            struct ne: public pkgversion {
                /// \sa pkgpattern::for_each
                template <typename Set, typename Function>
                void
                for_each(Set&& s, Function&& f, pkgbase const& base) const;

                /// Print a version constraint to an output stream.
                friend std::ostream&
                operator<< (std::ostream& out, ne const& ver) {
                    return out << "!=" << static_cast<pkgversion const&>(ver);
                }
            };
            /// Possible variants of version constraints.
            using constraint = std::variant<le, lt, ge, gt, eq, ne>;

            /// Parse a string representation of version constraints.
            version_range(std::string_view const& patstr);

            /// \sa pkgpattern::for_each
            template <typename Set, typename Function>
            void
            for_each(Set&& s, Function&& f) const;

            /// Print the version constraints to an output stream.
            friend std::ostream&
            operator<< (std::ostream& out, version_range const& ver);

            pkgbase base;   ///< The base name of package, e.g. \c foo
            constraint cst; ///< The version constraints, e.g. \c >=1.1<2
        };

        /// Glob pattern: foo-[0-9]*
        struct glob: std::string {
            /// \sa pkgpattern::for_each
            template <typename Set, typename Function>
            void
            for_each(Set&& s, Function&& f) const;
        };

        /// Possible variants of package name patterns.
        using pattern_type = std::variant<
            alternatives,
            version_range,
            glob
            >;

        /// Parse a package name pattern string.
        pkgpattern(std::string_view const& patstr);

        /** Apply a function \c f to each package matching to the pattern
         * in a set \c s. The set is expected to either be \c
         * std::set<pkgname> or <tt>std::map<pkgname, (anything)></tt>. The
         * function will be called with one argument, namely \c
         * Set::value_type& (or \c const& if \c s is \c const).
         */
        template <typename Set, typename Function>
        void
        for_each(Set&& s, Function&& f) const;

        /** Return \c Set::iterator (or \c Set::const_iterator if \c s is
         * \c const) for the best matching package that is found in a set
         * \c s. The set is expected to either be \c std::set<pkgname> or
         * <tt>std::map<pkgname, (anything)></tt>. If no packages match the
         * method returns <tt>s.end()</tt>.
         */
        template <typename Set>
        auto
        best(Set&& s) const;

        /// Turn an instance of \ref pkgpattern into a variant type.
        operator pattern_type () const {
            return _pat;
        }

        /// Print a string representation of \ref pkgpattern to an output
        /// stream.
        friend std::ostream&
        operator<< (std::ostream& out, pkgpattern const& pat);

    private:
        pattern_type _pat;
    };

    //
    // Implementation
    //

    template <typename Set, typename Function>
    void
    pkgpattern::alternatives::for_each(Set&& s, Function&& f) const {
        for (pkgpattern const& pat: _expanded) {
            pat.for_each(s, f);
        }
    }

    template <typename Set, typename Function>
    void
    pkgpattern::version_range::le::for_each(Set&& s, Function&& f, pkgbase const& base) const {
        for (auto it = s.lower_bound(pkgname(base, pkgversion()));
             it != s.end() &&
                 detail::pkgname_at(it).base == base &&
                 detail::pkgname_at(it).version <= *this;
             it++) {

            f(it);
        }
    }

    template <typename Set, typename Function>
    void
    pkgpattern::version_range::lt::for_each(Set&& s, Function&& f, pkgbase const& base) const {
        for (auto it = s.lower_bound(pkgname(base, pkgversion()));
             it != s.end() &&
                 detail::pkgname_at(it).base == base &&
                 detail::pkgname_at(it).version < *this;
             it++) {

            f(it);
        }
    }

    template <typename Set, typename Function>
    void
    pkgpattern::version_range::ge::for_each(Set&& s, Function&& f, pkgbase const& base) const {
        for (auto it = s.lower_bound(pkgname(base, min));
             it != s.end() &&
                 detail::pkgname_at(it).base == base &&
                 (!sup.has_value() ||
                  std::visit(
                      [&](auto const& ver) {
                          if constexpr (std::is_same_v<le const&, decltype(ver)>) {
                              return detail::pkgname_at(it).version <= ver;
                          }
                          else {
                              static_assert(std::is_same_v<lt const&, decltype(ver)>);
                              return detail::pkgname_at(it).version < ver;
                          }
                      },
                      *sup));
             it++) {

            f(it);
        }
    }

    template <typename Set, typename Function>
    void
    pkgpattern::version_range::gt::for_each(Set&& s, Function&& f, pkgbase const& base) const {
        for (auto it = s.upper_bound(pkgname(base, inf));
             it != s.end() &&
                 detail::pkgname_at(it).base == base &&
                 (!sup.has_value() ||
                  std::visit(
                      [&](auto const& ver) {
                          if constexpr (std::is_same_v<le const&, decltype(ver)>) {
                              return detail::pkgname_at(it).version <= ver;
                          }
                          else {
                              static_assert(std::is_same_v<lt const&, decltype(ver)>);
                              return detail::pkgname_at(it).version < ver;
                          }
                      },
                      *sup));
             it++) {

            f(it);
        }
    }

    template <typename Set, typename Function>
    void
    pkgpattern::version_range::eq::for_each(Set&& s, Function&& f, pkgbase const& base) const {
        // This is the best comparison! We only need to perform a search
        // just once!
        if (auto it = s.find(pkgname(base, *this)); it != s.end()) {
            f(it);
        }
    }

    template <typename Set, typename Function>
    void
    pkgpattern::version_range::ne::for_each(Set&& s, Function&& f, pkgbase const& base) const {
        for (auto it = s.lower_bound(pkgname(base, pkgversion()));
             it != s.end() &&
                 detail::pkgname_at(it).base == base;
             it++) {

            if (detail::pkgname_at(it).version != *this) {
                f(it);
            }
        }
    }

    template <typename Set, typename Function>
    void
    pkgpattern::version_range::for_each(Set&& s, Function&& f) const {
        std::visit(
            [&](auto const& ver) {
                ver.for_each(s, f, base);
            },
            cst);
    }

    template <typename Set, typename Function>
    void
    pkgpattern::glob::for_each(Set&& s, Function&& f) const {
        // Extract a literal part of the glob that precedes any meta
        // characters, and narrow down the search range with it. Much
        // better than just iterating the entire set.
        //
        // But since globs may be specified with or without a version
        // number, it may contain a hyphen that isn't a part of PKGBASE. So
        // we must treat the last occuring hyphen in the literal as a meta
        // character as well.
        auto literal =
            static_cast<std::string_view>(*this).substr(0, find_first_of("*?[]"));
        literal = literal.substr(0, literal.rfind('-'));

        for (auto it = s.lower_bound(pkgname(pkgbase(literal), pkgversion()));
             it != s.end() &&
                 starts_with(detail::pkgname_at(it).base, literal);
             it++) {

            auto const name_str = detail::pkgname_at(it).string();
            if (fnmatch(c_str(), name_str.c_str(), FNM_PERIOD) == 0) {
                f(it);
            }
            else {
                // The match may have failed only because the pattern lacks
                // version.
                auto const with_version =
                    static_cast<std::string>(*this) + "-[0-9]*";
                if (fnmatch(with_version.c_str(), name_str.c_str(), FNM_PERIOD) == 0) {
                    f(it);
                }
            }
        }
    }

    template <typename Set, typename Function>
    void
    pkgpattern::for_each(Set&& s, Function&& f) const {
        std::visit(
            [&](auto const& pat) {
                pat.for_each(s, f);
            },
            _pat);
    }

    template <typename Set>
    auto
    pkgpattern::best(Set&& s) const {
        std::optional<
            std::reference_wrapper<pkgname const>> current;
        for_each(
            s,
            [&](auto&& it) {
                if (!current.has_value() || *current < detail::pkgname_at(it)) {
                    current.emplace(std::cref(detail::pkgname_at(it)));
                }
            });

        if (current.has_value()) {
            return s.find(*current);
        }
        else {
            return s.end();
        }
    }
}
