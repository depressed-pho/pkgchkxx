#pragma once

namespace pkgxx {
    /** Define != in terms of ==. */
    template <typename T>
    struct equality_comparable {
        friend constexpr bool
        operator!= (const T& a, const T& b) {
            return !(a == b);
        }
    };

    /** Define >, <=, and >= in terms of == and <. */
    template <typename T>
    struct ordered: public equality_comparable<T> {
        friend constexpr bool
        operator> (const T& a, const T& b) {
            return b < a;
        }

        friend constexpr bool
        operator<= (const T& a, const T& b) {
            return a < b || a == b;
        }

        friend constexpr bool
        operator>= (const T& a, const T& b) {
            return b < a || a == b;
        }
    };
}
