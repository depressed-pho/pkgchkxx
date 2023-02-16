#pragma once

#include <exception>
#include <filesystem>
#include <iterator>
#include <optional>

#include "environment.hxx"
#include "harness.hxx"
#include "ordered.hxx"
#include "pkgpath.hxx"
#include "pkgname.hxx"

namespace pkg_chk {
    struct bad_iteration: std::exception {
        using std::exception::exception;
    };

    namespace detail {
        template <typename Source>
        struct pkg_info_iterator: equality_comparable<pkg_info_iterator<Source>> {
            using iterator_category = std::input_iterator_tag;
            using value_type        = typename Source::value_type;
            using pointer           = value_type*;
            using reference         = value_type&;

            pkg_info_iterator()
                : _source(nullptr) {}

            pkg_info_iterator(Source& source)
                : _source(&source)
                , _current(_source->read_next()) {}

            bool
            operator== (pkg_info_iterator const& other) const noexcept {
                if (_current.has_value()) {
                    return _source == other._source && other._current.has_value();
                }
                else {
                    return !other._current.has_value();
                }
            }

            reference
            operator* () {
                return _current.value();
            }

            pointer
            operator-> () {
                _current.value();
                return _current.operator->();
            }

            pkg_info_iterator&
            operator++ () {
                _current = _source->read_next();
                return *this;
            }

            pkg_info_iterator
            operator++ (int) {
                pkg_info_iterator it = *this;
                ++(*this);
                return it;
            }

        private:
            Source* _source;
            std::optional<value_type> _current;
        };

        template <typename Derived>
        struct pkg_info_reader_base {
            using iterator = detail::pkg_info_iterator<Derived>;

            pkg_info_reader_base()
                : _began(false) {}

            /** begin() can be called only once. Calling it twice will
             * throw 'bad_iteration'. */
            iterator
            begin() {
                if (_began) {
                    throw bad_iteration();
                }
                else {
                    _began = true;
                    return iterator(*static_cast<Derived*>(this));
                }
            }

            iterator
            end() noexcept {
                return iterator();
            }

        private:
            bool _began;
        };
    }

    /** This is a read-only container object representing a sequence of
     * PKGNAME of installed packages. */
    struct installed_pkgnames: public detail::pkg_info_reader_base<installed_pkgnames> {
        using value_type = pkgname;

        installed_pkgnames(environment const& env);

    private:
        friend class detail::pkg_info_iterator<installed_pkgnames>;

        std::optional<value_type>
        read_next();

    private:
        harness _pkg_info;
    };

    /** This is a read-only container object representing a sequence of
     * PKGPATH of installed packages. */
    struct installed_pkgpaths: public detail::pkg_info_reader_base<installed_pkgpaths> {
        using value_type = pkgpath;

        installed_pkgpaths(environment const& env);

    private:
        friend class detail::pkg_info_iterator<installed_pkgpaths>;

        std::optional<value_type>
        read_next();

    private:
        harness _pkg_info;
    };
}
