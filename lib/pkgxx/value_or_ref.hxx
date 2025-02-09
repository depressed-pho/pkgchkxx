#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

#include <pkgxx/always_false_v.hxx>

namespace pkgxx {
    /** \c value_or_ref<T> contains either a value of type \c T or a
     * reference to \c T, depending on how it is constructed. \c T must not
     * be a reference type, but it may be a pointer type or an array type.
     */
    template <typename T>
    struct value_or_ref {
        static_assert(!std::is_reference_v<T>, "type T must not be a reference type");

        /** \c value_or_ref<T> isn't default-constructible.
         */
        value_or_ref() = delete;

        /** \c value_or_ref<T> is copyable iff \c T is copyable.
         */
        value_or_ref(value_or_ref<T> const& other)
            : _v([&] {
                     return std::visit(
                         [](auto&& arg) {
                             using Arg = std::decay_t<decltype(arg)>;
                             if constexpr (std::is_same_v<Arg, std::unique_ptr<T>>) {
                                 return std::make_unique<T>(*arg);
                             }
                             else if constexpr (std::is_same_v<Arg, std::reference_wrapper<T>>) {
                                 return arg;
                             }
                             else {
                                 static_assert(always_false_v<Arg>);
                             }
                         }, other._v);
                 }()) {}

        /** \c value_or_ref<T> is movable even if \c T is not.
         */
        value_or_ref(value_or_ref<T>&& other)
            : _v(std::move(other._v)) {}

        /** Instantiate \ref value_or_ref by borrowing a value of type \c T
         * as a reference.
         */
        value_or_ref(T& val)
            : _v(std::ref(val)) {}

        /** Instantiate \ref value_or_ref by copying a value of type \c
         * T. Ill-formed if \c T is not copyable.
         */
        template <typename T_ = T,
                  typename = std::enable_if_t<!std::is_const_v<T_>>>
        value_or_ref(T const& val)
            : _v(std::make_unique<T>(val)) {}

        /** Instantiate \ref value_or_ref by moving a value of type \c
         * T. Ill-formed if \c T is not movable.
         */
        value_or_ref(T&& val)
            : _v(std::make_unique<T>(std::move(val))) {}

        /** Instantiate \ref value_or_ref by initialising a value of type
         * \c U in-place with given arguments. Ill-formed if \c U is not
         * implicitly convertible to \c T.
         */
        template <typename U, typename... Args>
        explicit value_or_ref(std::in_place_type_t<U>, Args&&... args)
            : _v(std::make_unique<U>(std::forward<Args>(args)...)) {}

        /** Synonym to operator->
         */
        T*
        get() noexcept {
            return this->operator->();
        }

        /** Synonym to operator->
         */
        std::add_pointer_t<std::add_const_t<T>>
        get() const noexcept {
            return this->operator->();
        }

        /** Return a potentially mutable pointer to \c T. It returns a
         * const pointer if \c T is const.
         */
        T*
        operator-> () noexcept {
            // This const_cast is safe because this method takes a mutable
            // "this" pointer.
            return const_cast<T*>(
                static_cast<value_or_ref<T> const*>(this)->operator-> ());
        }

        /** Return a const pointer to \c T.
         */
        std::add_pointer_t<std::add_const_t<T>>
        operator-> () const noexcept {
            return std::visit(
                [](auto&& arg) {
                    using Arg = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<Arg, std::unique_ptr<T>>) {
                        return arg.get();
                    }
                    else if constexpr (std::is_same_v<Arg, std::reference_wrapper<T>>) {
                        return &arg.get();
                    }
                    else {
                        static_assert(always_false_v<Arg>);
                    }
                }, _v);
        }

        /** Synonym to operator*
         */
        T&
        value() noexcept {
            return this->operator*();
        }

        /** Synonym to operator*
         */
        std::add_lvalue_reference_t<std::add_const_t<T>>
        value() const noexcept {
            return this->operator*();
        }

        /** Return a potentially mutable reference to \c T. It returns a
         * const reference if \c T is const.
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
        std::add_lvalue_reference_t<std::add_const_t<T>>
        operator* () const noexcept {
            return std::visit(
                [](auto&& arg) -> std::add_lvalue_reference_t<std::add_const_t<T>> {
                    using Arg = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<Arg, std::unique_ptr<T>>) {
                        return *arg;
                    }
                    else if constexpr (std::is_same_v<Arg, std::reference_wrapper<T>>) {
                        return arg.get();
                    }
                    else {
                        static_assert(always_false_v<Arg>);
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
