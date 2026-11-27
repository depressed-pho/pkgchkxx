#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <filesystem>
#include <iterator>
#include <optional>
#include <pkgxx/numeral.hxx>
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
            dependency() {}

            dependency(pkgxx::pkgpattern&& pattern,
                       pkgxx::pkgpath&& path)
                : pattern{std::move(pattern)}
                , path{std::move(path)} {}

            pkgxx::pkgpattern pattern;
            pkgxx::pkgpath    path;

            template <typename Archive>
            void
            serialize(Archive& ar, std::uint32_t const version) {
                switch (version) {
                case 0:
                    ar(cereal::make_nvp("pattern", pattern));
                    ar(cereal::make_nvp("pkgpath", path));
                    break;
                default:
                    throw std::runtime_error{"dependency version incompatible"};
                }
            }
        };

        struct variables {
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
            // Don't care about INDIRECT_BUILD_DEPENDS and
            // INDIRECT_DEPENDS. They will appear somewhere in
            // BUILD_DEPENDS or DEPENDS anyway.

            template <typename Archive>
            void
            serialize(Archive& ar, std::uint32_t const version) {
                switch (version) {
                case 0:
                    ar(cereal::make_nvp("pkgname"          , PKGNAME));
                    ar(cereal::make_nvp("broken"           , BROKEN));
                    ar(cereal::make_nvp("bootstrap-depends", BOOTSTRAP_DEPENDS));
                    ar(cereal::make_nvp("tool-depends"     , TOOL_DEPENDS));
                    ar(cereal::make_nvp("build-depends"    , BUILD_DEPENDS));
                    ar(cereal::make_nvp("depends"          , DEPENDS));
                    break;
                default:
                    throw std::runtime_error{"variables version incompatible"};
                }
            }
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
        enum class format {
            bin,
            xml
        };

    private:
        /** Constructing an empty database is most likely a mistake. */
        lintdb() = default;

    public:
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

        /** Load a database from file. */
        lintdb(std::filesystem::path const& path);

        /** Return an unspecified iterable object that can be used for
         * iterating over \c db::variables for each package in the
         * database.
         */
        db::detail::packages
        packages() const& {
            return db::detail::packages{_repr};
        }

        /** Export the database to the given file path. */
        void
        save(std::filesystem::path const& path, format fmt = format::bin) const;

    private:
        friend class cereal::access;

        /* Represent lintdb as like a map<pkgpath, vector<variables>>. No
         * need to serialise pkgbase separately, because it's also in
         * variables::PKGNAME. We could actually construct the map and
         * serialise it, but that would be expensive. We can do
         * better. Let's represent lintdb as a custom variable-sized
         * container and serialize its contents dynamically. The resulting
         * structure is as follows:
         *
         * <lintdb size="dynamic">
         *   <value0>
         *     <pkgpath>pkgtools/pkgchkxx</pkgpath>
         *     <packages size="dynamic">
         *       <value0>
         *         <pkgname>lzip-1.26</pkgname>
         *         <broken size="dynamic"/>
         *         <depends size="dynamic">
         *           <value0>
         *             <pattern>pkgconf>=1.9.4</pattern>
         *             <pkgpath>devel/pkgconf</pkgpath>
         *           </value0>
         *         </depends>
         *       </value0>
         *     </packages>
         *   </value0>
         * </lintdb>
         */

        template <bool is_ref>
        struct pkgs_helper {

            using pkgs_type =
                std::conditional_t<
                    is_ref,
                    std::unordered_map<pkgxx::pkgbase, db::variables> const&,
                    std::unordered_map<pkgxx::pkgbase, db::variables>
                    >;

            template <bool B = is_ref, typename = std::enable_if_t<B>>
            pkgs_helper(pkgs_type const& pkgs)
                : pkgs{pkgs} {}

            template <bool B = is_ref, typename = std::enable_if_t<!B>>
            pkgs_helper() {}

            template <typename Archive>
            void
            save(Archive& ar) const {
                ar(cereal::make_size_tag(static_cast<cereal::size_type>(pkgs.size())));
                for (auto const& [_, vars]: pkgs) {
                    ar(vars);
                }
            }

            template <typename Archive>
            void
            load(Archive& ar) {
                cereal::size_type size;
                ar(cereal::make_size_tag(size));
                pkgs.reserve(static_cast<typename pkgs_type::size_type>(size));
                for (cereal::size_type i = 0; i < size; i++) {
                    db::variables vars;
                    ar(vars);
                    pkgxx::pkgbase base = vars.PKGNAME.base; // Copy it.
                    pkgs.emplace(std::move(base), std::move(vars));
                }
            }

            pkgs_type pkgs;
        };

        /// If it's called with a single argument, is_ref should be deduced
        /// to true.
        template <typename Pkgs>
        pkgs_helper(Pkgs const&) -> pkgs_helper<true>;

        template <bool is_ref>
        struct path_pkgs_helper {

            using path_type =
                std::conditional_t<
                    is_ref,
                    pkgxx::pkgpath const&,
                    pkgxx::pkgpath
                    >;

            using ph_type =
                std::conditional_t<
                    is_ref,
                    pkgs_helper<is_ref> const&,
                    pkgs_helper<is_ref>
                    >;

            template <bool B = is_ref, typename = std::enable_if_t<B>>
            path_pkgs_helper(path_type const& path, ph_type const& ph)
                : path{path}
                , ph{ph} {}

            template <bool B = is_ref, typename = std::enable_if_t<!B>>
            path_pkgs_helper() {}

            template <typename Archive>
            void
            serialize(Archive& ar) {
                ar(cereal::make_nvp("pkgpath", path));
                ar(cereal::make_nvp("packages", ph));
            }

            path_type path;
            ph_type ph;
        };

        /// If it's called with two arguments, is_ref should be deduced to
        /// true.
        template <typename PathType, typename PHType>
        path_pkgs_helper(PathType const&, PHType const&) -> path_pkgs_helper<true>;

        /// If it's called with no arguments, is_ref should be deduced to
        /// false. Clang emits a warning about this rule, caliming it isn't
        /// needed, but that's wrong. Removing it makes
        /// "path_pkgs_helper{}" fail to compile.
        path_pkgs_helper() -> path_pkgs_helper<false>;

        /** Serialise the entire database. */
        template <typename Archive>
        void
        save(Archive& ar, std::uint32_t const) const {
            ar(cereal::make_size_tag(static_cast<cereal::size_type>(_repr.size())));
            for (auto const& [path, pkgs]: _repr) {
                ar(path_pkgs_helper{path, pkgs_helper{pkgs}});
            }
        }

        /** Deserialise the entire database. */
        template <typename Archive>
        void
        load(Archive& ar, std::uint32_t const version) {
            switch (version) {
            case 0:
                {
                    cereal::size_type size;
                    ar(cereal::make_size_tag(size));
                    _repr.reserve(static_cast<db::detail::repr_type::size_type>(size));
                    for (cereal::size_type i = 0; i < size; i++) {
                        path_pkgs_helper pph;
                        ar(pph);
                        _repr.emplace(std::move(pph.path), std::move(pph.ph.pkgs));
                    }
                }
                break;
            default:
                throw std::runtime_error{"lintdb version incompatible"};
            }
        }

    private:
        lintdb(
            int, // a dummy parameter to avoid conflicting with the public ctor
            std::filesystem::path const& pkgsrcdir,
            std::vector<pkgxx::pkgpath> const& pkgpaths,
            pkgxx::value_or_ref<pkgxx::maybe_ttystream>&& output,
            bool list_makefiles,
            pkgxx::positive<unsigned> const& concurrency);

        static pkgxx::value_or_ref<pkgxx::maybe_ttystream>
        default_output();

        static pkgxx::positive<unsigned>
        default_concurrency();

        db::detail::repr_type _repr;
    };
}
