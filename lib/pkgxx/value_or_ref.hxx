#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

namespace pkgxx {
    /** \c value_or_ref<T> contains either a value of type \c T or a
     * reference to \c T, depending on how it is constructed. \c T cannot
     * be \c const.
     */
    template <typename T>
    struct value_or_ref {
        static_assert(!std::is_const_v<T>);

        /** \c value_or_ref<T> isn't default-constructible.
         */
        value_or_ref() = delete;

        /** \c value_or_ref<T> is copyable iff \c T is copyable.
         */
        template <typename T_ = T,
                  typename = std::enable_if_t<std::is_copy_constructible_v<T_>>>
        value_or_ref(value_or_ref<T> const& other)
            : _v(std::make_unique(*other)) {}

        /** \c value_or_ref<T> is movable even if \c T is not.
         */
        value_or_ref(value_or_ref<T>&& other)
            : _v(std::move(other._v)) {}

        /** Instantiate \ref value_or_ref by borrowing a value of type \c T
         * as a reference.
         */
        value_or_ref(T& val)
            : _v(std::ref(val)) {}

        /** Instantiate \ref value_or_ref by moving a value of type \c
         * T. Ill-formed if \c T is not movable.
         */
        template <typename T_ = T,
                  typename = std::enable_if_t<std::is_move_constructible_v<T_>>>
        value_or_ref(T&& val)
            : _v(std::make_unique(std::move(val))) {}

        /** Instantiate \ref value_or_ref by initialising a value of type
         * \c U in-place with given arguments.
         */
        template <typename U, typename... Args>
        explicit value_or_ref(std::in_place_type_t<U>, Args&&... args)
            : _v(std::make_unique<U>(std::forward<Args>(args)...)) {}

        /** Return a mutable pointer to \c T.
         */
        T*
        get() noexcept {
            // This const_cast is safe because this method takes a mutable
            // "this" pointer.
            return const_cast<T*>(
                static_cast<value_or_ref<T> const*>(this)->get());
        }

        /** Return a const pointer to \c T.
         */
        T const*
        get() const noexcept {
            return std::visit(
                [this](auto&& arg) -> T const* {
                    using Arg = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<Arg, std::unique_ptr<T>>) {
                        return arg.get();
                    }
                    else {
                        static_assert(std::is_same_v<Arg, std::reference_wrapper<T>>);
                        return &arg.get();
                    }
                }, _v);
        }

        /** Return a mutable reference to \c T.
         */
        T&
        operator* () noexcept {
            // This const_cast is safe because this method takes a mutable
            // "this" pointer.
            return const_cast<T&>(
                static_cast<value_or_ref<T> const*>(this)->operator* ());
        }

        /** Return a const reference to \c T.
         */
        T const&
        operator* () const noexcept {
            return std::visit(
                [this](auto&& arg) -> T const& {
                    using Arg = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<Arg, std::unique_ptr<T>>) {
                        return *arg;
                    }
                    else {
                        static_assert(std::is_same_v<Arg, std::reference_wrapper<T>>);
                        return arg.get();
                    }
                }, _v);
        }

    private:
        std::variant<
            std::unique_ptr<T>,
            std::reference_wrapper<T>
            > _v;
    };
}
