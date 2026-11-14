#include <filesystem>
#include <iterator>
#include <optional>
#include <pkgxx/ordered.hxx>
#include <pkgxx/pkgname.hxx>
#include <pkgxx/pkgpath.hxx>
#include <pkgxx/pkgpattern.hxx>
#include <pkgxx/tty.hxx>
#include <pkgxx/value_or_ref.hxx>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// We know what we are doing! Just don't warn us about these!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-string-literal-operator-template"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <named-parameters.hpp>
#pragma GCC diagnostic pop

namespace lint {
    namespace db {
        struct dependency {
            dependency(pkgxx::pkgpattern pattern,
                       pkgxx::pkgpath    path)
                : pattern{pattern}
                , path{path} {}

            pkgxx::pkgpattern pattern;
            pkgxx::pkgpath    path;
        };

        struct variables {
            variables(pkgxx::pkgname const& pkgname)
                : PKGNAME(pkgname) {}

            variables(pkgxx::pkgname&& pkgname)
                : PKGNAME(std::move(pkgname)) {}

            pkgxx::pkgname PKGNAME;

            /** The values of BROKEN with quotes and escapes interpreted as
             * shells do.
             */
            std::vector<std::string> BROKEN;

            // These sets are represented as vectors because pkgpattern
            // isn't equality-comparable nor ordered.
            std::vector<dependency> BOOTSTRAP_DEPENDS;
            std::vector<dependency> TOOL_DEPENDS;
            std::vector<dependency> BUILD_DEPENDS;
            std::vector<dependency> DEPENDS;
            std::vector<dependency> INDIRECT_BUILD_DEPENDS;
            std::vector<dependency> INDIRECT_DEPENDS;
        };
    }

    namespace detail {
        template <typename Container>
        std::vector<typename std::decay_t<Container>::value_type>
        to_vector(Container&& from) {
            using ret_t = decltype(to_vector(std::forward<Container>(from)));
            if constexpr (std::is_same_v<decltype(from), ret_t>) {
                // It's already a vector, not even a reference.
                return from;
            }
            else if constexpr (std::is_same_v<std::decay_t<decltype(from)>, ret_t>) {
                // It's a vector but is a reference of some kind.
                return ret_t(std::forward<Container>(from));
            }
            else {
                // It's not a vector so construct one by copying elements.
                return ret_t(from.begin(), from.end());
            }
        }
    }

    namespace db {
        namespace detail {
            // Internal representation of lintdb.
            using repr_type =
                std::unordered_map<
                    pkgxx::pkgpath,
                    std::unordered_map<pkgxx::pkgbase, variables>
                    >;

            struct pkg_iterator: public pkgxx::equality_comparable<pkg_iterator> {
                using value_type = variables;

                pkg_iterator() {}
                pkg_iterator(repr_type const& ref);

                [[gnu::pure]]
                bool
                operator== (pkg_iterator const& other) const noexcept;

                [[gnu::pure]]
                value_type const&
                operator* () const;

                pkg_iterator
                operator++ ();

                pkg_iterator
                operator++ (int);

            private:
                repr_type const* _ref;
                std::optional<repr_type::const_iterator> _cur_bases;
                std::optional<repr_type::mapped_type::const_iterator> _cur_vars;
            };

            struct packages {
                using const_iterator = pkg_iterator const;

                packages(repr_type const& ref) noexcept
                    : _ref(ref) {}

                const_iterator
                begin() const noexcept {
                    return pkg_iterator{_ref};
                }

                const_iterator
                end() const noexcept {
                    return pkg_iterator{};
                }

            private:
                repr_type const& _ref;
            };
        }
    }

    // I'm not comfortable with bringing these in this scope, but what else
    // can we do?
    using namespace na::literals;

    struct lintdb {
        /** Construct a database by scanning Makefile's. \c Paths is a
         * container of \c pkgxx::pkgpath providing the set of PKGPATHs to
         * scan.
         */
        template <typename Paths, typename... Args>
        lintdb(std::filesystem::path const& pkgsrcdir, Paths&& pkgpaths, Args&&... args)
            : lintdb(
                0, pkgsrcdir, detail::to_vector(std::forward<Paths>(pkgpaths)),
                static_cast<pkgxx::value_or_ref<pkgxx::maybe_ttystream>&&>(
                    na::get("output"_na = default_output(), std::forward<Args>(args)...)),
                na::get("list_makefiles"_na = false                , std::forward<Args>(args)...),
                na::get("concurrency"_na    = default_concurrency(), std::forward<Args>(args)...)) {}

        /** Return an unspecified iterable object that can be used for
         * iterating over \c db::variables for each package in the
         * database.
         */
        db::detail::packages
        packages() const& {
            return db::detail::packages{_repr};
        }

    private:
        lintdb(
            int, // a dummy parameter to avoid conflicting with the public ctor
            std::filesystem::path const& pkgsrcdir,
            std::vector<pkgxx::pkgpath> const& pkgpaths,
            pkgxx::value_or_ref<pkgxx::maybe_ttystream>&& output,
            bool list_makefiles,
            unsigned concurrency);

        static pkgxx::value_or_ref<pkgxx::maybe_ttystream>
        default_output();

        static unsigned
        default_concurrency();

        db::detail::repr_type _repr;
    };
}
