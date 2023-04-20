#pragma once

#include <functional>

namespace pkgxx {
    /// to work around static_assert(false, ...)
    template <typename T>
    struct dependent_false: std::false_type {};

    /// \sa https://github.com/HowardHinnant/hash_append/issues/7
    template <typename T>
    inline void hash_append(std::size_t& seed, const T& value) {
        if constexpr (sizeof(std::size_t) == 4) {
            seed ^= std::hash<T>{}(value) + 0x9e3779b9U + (seed << 6) + (seed >> 2);
        }
        else if constexpr (sizeof(std::size_t) == 8) {
            seed ^= std::hash<T>{}(value) + 0x9e3779b97f4a7c15LLU + (seed << 12) +
                                      (seed >> 4);
        }
        else {
            static_assert(dependent_false<T>::value, "hash_combine not implemented");
        }
    }

    /// \sa https://github.com/HowardHinnant/hash_append/issues/7
    template <typename... Types>
    std::size_t
    hash_combine(const Types&... args) {
        std::size_t seed = 0;
        (hash_append(seed, args),...); // create hash value with seed over all args
        return seed;
    }
}

/// \c std::hash for \c std::pair is not defined in the standard
/// library. No idea why.
template <typename T1, typename T2>
struct std::hash<std::pair<T1, T2>> {
    std::size_t
    operator() (std::pair<T1, T2> const& pair) const noexcept {
        return pkgxx::hash_combine(pair.first, pair.second);
    }
};
