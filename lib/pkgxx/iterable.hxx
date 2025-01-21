#pragma once

namespace pkgxx {
    namespace detail {
        template <typename T>
        struct reversion_wrapper {
            T& iterable;
        };

        template <typename T>
        auto begin (reversion_wrapper<T> const& w) {
            return std::rbegin(w.iterable);
        }

        template <typename T>
        auto end (reversion_wrapper<T> const& w) {
            return std::rend(w.iterable);
        }
    }

    /** \c std::ranges::reverse_view for non-C++20 compilers. */
    template <typename T>
    detail::reversion_wrapper<T> reverse(T&& iterable) {
        return { iterable };
    }
}
