#pragma once

#include <memory>
#include <set>
#include <type_traits>

#include <pkgxx/harness.hxx>
#include <pkgxx/ordered.hxx>
#include <pkgxx/pkgname.hxx>
#include <pkgxx/pkgpattern.hxx>

namespace pkgxx {
    /** An iterator that iterates through installed packages. */
    struct installed_pkgname_iterator: equality_comparable<installed_pkgname_iterator> {
        using iterator_category = std::forward_iterator_tag; ///< The category of the iterator.
        using value_type        = pkgxx::pkgname;            ///< The value of the iterator.
        using pointer           = value_type*;               ///< The pointer type of the iterator.
        using reference         = value_type&;               ///< The reference type of the iterator.

        /// Construct an invalid iterator that acts as \c .end()
        installed_pkgname_iterator() {}

        /// Construct an iterator pointing at the first installed
        /// package.
        installed_pkgname_iterator(std::string const& PKG_INFO);

        /// Iterator equality.
        bool
        operator== (installed_pkgname_iterator const& other) const noexcept {
            if (_current.has_value()) {
                return _pkg_info == other._pkg_info && other._current.has_value();
            }
            else {
                return !other._current.has_value();
            }
        }

        /// Iterator dereference.
        reference
        operator* () {
            return _current.value();
        }

        /// Iterator dereference.
        pointer
        operator-> () {
            _current.value();
            return _current.operator->();
        }

        /// Iterator incrementation.
        installed_pkgname_iterator&
        operator++ ();

        /// Iterator incrementation.
        installed_pkgname_iterator
        operator++ (int) {
            auto it = *this;
            ++(*this);
            return it;
        }

    private:
        std::shared_ptr<pkgxx::harness> _pkg_info;
        std::optional<value_type> _current;
    };

    /// A container-like type representing the set of installed packages.
    struct installed_pkgnames {
        /// The iterator that iterates through installed packages.
        using const_iterator = installed_pkgname_iterator;

        /// Construct an instance of \ref installed_pkgnames. This class
        /// acts as a container having the set of installed packages.
        installed_pkgnames(std::string const& PKG_INFO)
            : _pkg_info(PKG_INFO) {}

        /// Return an iterator to the first package.
        const_iterator
        begin() const noexcept {
            return const_iterator(_pkg_info);
        }

        /// Return an iterator past the last package.
        const_iterator
        end() const noexcept {
            return const_iterator();
        }

    private:
        std::string _pkg_info;
    };

    /** An iterator that iterates through build information for a package. */
    struct build_info_iterator: equality_comparable<build_info_iterator> {
        using iterator_category = std::forward_iterator_tag; ///< The category of the iterator.
        using value_type        =
            std::pair<
                std::string_view,
                std::string_view>;                           ///< The value of the iterator.
        using pointer           = value_type*;               ///< The pointer type of the iterator.
        using reference         = value_type&;               ///< The reference type of the iterator.

        /// Construct an invalid iterator that acts as \c .end()
        build_info_iterator() {}

        /// Construct an iterator pointing at the first variable.
        build_info_iterator(std::string const& PKG_INFO, pkgxx::pkgpattern const& pattern);

        /// Iterator equality.
        bool
        operator== (build_info_iterator const& other) const noexcept {
            if (_current.has_value()) {
                return _pkg_info == other._pkg_info && other._current.has_value();
            }
            else {
                return !other._current.has_value();
            }
        }

        /// Iterator dereference.
        reference
        operator* () {
            return _current.value();
        }

        /// Iterator dereference.
        pointer
        operator-> () {
            _current.value();
            return _current.operator->();
        }

        /// Iterator incrementation.
        build_info_iterator&
        operator++ ();

        /// Iterator incrementation.
        build_info_iterator
        operator++ (int) {
            auto it = *this;
            ++(*this);
            return it;
        }

    private:
        std::shared_ptr<pkgxx::harness> _pkg_info;
        std::string _current_line;
        std::optional<value_type> _current;
    };

    /// A container-like type representing build information for a package.
    struct build_info {
        /// The iterator that iterates through build information variables.
        using const_iterator = build_info_iterator;

        /// Construct an instance of \ref build_info for a package name.
        build_info(std::string const& PKG_INFO, pkgxx::pkgname const& name)
            : _pkg_info(PKG_INFO)
            , _pattern(name) {}

        /// Construct an instance of \ref build_info for a package base.
        build_info(std::string const& PKG_INFO, pkgxx::pkgbase const& base)
            : _pkg_info(PKG_INFO)
            , _pattern(base) {}

        /// Return an iterator to the given variable, or \c end() if no
        /// such variable exists.
        const_iterator
        find(std::string_view const& var) const;

        /// Return an iterator to the first variable.
        const_iterator
        begin() const noexcept {
            return const_iterator(_pkg_info, _pattern);
        }

        /// Return an iterator past the last package.
        const_iterator
        end() const noexcept {
            return const_iterator();
        }

    private:
        std::string _pkg_info;
        pkgxx::pkgpattern _pattern;
    };

    namespace detail {
        bool
        is_pkg_installed(std::string const& PKG_INFO, pkgxx::pkgpattern const& pat);
    }

    /// Check if a package is installed. \c Name must either be a \ref
    /// pkgxx::pkgbase or \ref pkgxx::pkgname.
    template <typename Name>
    inline bool
    is_pkg_installed(std::string const& PKG_INFO, Name const& name) {
        return detail::is_pkg_installed(PKG_INFO, pkgxx::pkgpattern(name));
    }

    namespace detail {
        std::set<pkgxx::pkgname>
        build_depends(std::string const& PKG_INFO, pkgxx::pkgpattern const& pat);
    }

    /// Obtain the set of \c \@blddep entries of an installed package. \c
    /// Name must either be a \ref pkgxx::pkgbase or \ref
    /// pkgxx::pkgname. This includes \c BUILD_DEPENDS and \c DEPENDS but
    /// not \c TOOL_DEPENDS.
    template <typename Name>
    inline std::set<pkgxx::pkgname>
    build_depends(std::string const& PKG_INFO, Name const& name) {
        return detail::build_depends(PKG_INFO, pkgxx::pkgpattern(name));
    }

    namespace detail {
        std::set<pkgxx::pkgname>
        who_requires(std::string const& PKG_INFO, pkgxx::pkgpattern const& pat);
    }

    /// Obtain the set of packages which has a run-time dependency on the
    /// given one. \c Name must either be a \ref pkgxx::pkgbase or \ref
    /// pkgxx::pkgname.
    template <typename Name>
    inline std::set<pkgxx::pkgname>
    who_requires(std::string const& PKG_INFO, Name const& name) {
        return detail::who_requires(PKG_INFO, pkgxx::pkgpattern(name));
    }
}
