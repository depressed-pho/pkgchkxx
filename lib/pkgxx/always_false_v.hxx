#pragma once

#include <type_traits>

namespace pkgxx {
    /** A type-dependent boolean constant, taken from
     * [https://stackoverflow.com/a/76675119]. We can get rid of this when
     * we switch to C++23.
     */
    template <typename>
    using always_false = std::bool_constant<false>;

    template <typename T>
    inline constexpr bool always_false_v = always_false<T>::value;
}
