#pragma once

#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace pkgxx {
    /** \c positive<T> wraps an arithmetic type \c T (i.e. either an
     * integral or a floating-point type) and restricts its value to be
     * positive.
     */
    template <typename T>
    struct positive {
        static_assert(
            std::is_arithmetic_v<T>,
            "pkgxx::positive<T> takes an arithmetic type T");

        /** \c positive<T> is default-constructible iff \c T is an integral
         * type. Its default value is \c 1.
         */
        template <typename = std::enable_if_t<std::is_integral_v<T>>>
        positive()
            : _val{1} {}

        positive(positive<T> const&) = default;
        positive(positive<T>&&)      = default;

        /** Initialising \c positive<T> with a non-positive value will
         * throw a \c std::range_error.
         */
        template<typename U,
                 typename = std::enable_if_t<std::is_constructible_v<T, U&&>>>
        positive(U&& val) {
            if (val > static_cast<std::decay_t<U>>(0)) {
                _val = val;
            }
            else {
                throw std::range_error{"Only a positive value is allowed"};
            }
        }

        positive<T>&
        operator= (positive<T> const&) = default;

        positive<T>&
        operator= (positive<T>&&) = default;

        /** Assigning a non-positive value to \c positive<T> will throw a
         * \c std::range_error.
         */
        template<typename U,
                 typename = std::enable_if_t<std::is_assignable_v<T, U&&>>>
        positive<T>&
        operator= (U&& val) {
            if (val > static_cast<std::decay_t<U>>(0)) {
                _val = val;
                return *this;
            }
            else {
                throw std::range_error{"Only a positive value is allowed"};
            }
        }

        /** \c positive<T> can be implicitly casted to its underlying type
         * \c T.
         */
        operator T () const noexcept {
            return _val;
        }

        /** \c positive<T> can be written to a \c std::basic_ostream as
         * long as \c T can be.
         */
        template <typename CharT, typename Traits>
        friend
        std::basic_ostream<CharT, Traits>&
        operator<< (std::basic_ostream<CharT, Traits>& out, positive<T> const& val) {
            return out << val._val;
        }

        /** \c positive<T> can be read from a \c std::basic_istream as long
         * as \c T can be. However, when the read value is non-positive,
         * the value will be discarded and the failbit will be set.
         */
        template <typename CharT, typename Traits>
        friend
        std::basic_istream<CharT, Traits>&
        operator>> (std::basic_istream<CharT, Traits>& in, positive<T>& val) {
            T tmp;
            in >> tmp;
            if (!in.fail()) {
                if (tmp > static_cast<T>(0)) {
                    val._val = tmp;
                }
                else {
                    in.setstate(std::ios_base::failbit);
                }
            }
            return in;
        }

    private:
        T _val;
    };
}
