#pragma once

namespace pkgxx {
    /** Define the operator \c != in terms of \c ==.
     */
    template <typename T>
    struct equality_comparable {
        /** Define <tt>a != b</tt> as <tt>!(a == b)</tt>.
         */
        friend constexpr bool
        operator!= (const T& a, const T& b) {
            return !(a == b);
        }
    };

    /** Define operators \c >, \c <=, and \c >= in terms of \c == and \c
     * <.
     */
    template <typename T>
    struct ordered: public equality_comparable<T> {
        /** Define <tt>a > b</tt> as <tt>b < a</tt>.
         */
        friend constexpr bool
        operator> (const T& a, const T& b) {
            return b < a;
        }

        /** Define <tt>a <= b</tt> as <tt>a < b || a == b</tt>.
         */
        friend constexpr bool
        operator<= (const T& a, const T& b) {
            return a < b || a == b;
        }

        /** Define <tt>a >= b</tt> as <tt>b < a || a == b</tt>.
         */
        friend constexpr bool
        operator>= (const T& a, const T& b) {
            return b < a || a == b;
        }
    };
}
