#pragma once

#include <atomic>
#include <cassert>
#include <type_traits>
#include <utility>

namespace pkgxx {
    /** This is a variant of std::shared_ptr that permits the destructor of
     * the contained value to throw exceptions. It can only be used if \c T
     * is a complete type. Use this sparingly, as throwing destructor isn't
     * a good idea in the first place.
     */
    template <typename T>
    struct permissive_shared_ptr {
        using element_type = T;

        permissive_shared_ptr() noexcept
            : _box(nullptr) {}

        permissive_shared_ptr(T* ptr)
            : _box(new box(ptr)) {}

        permissive_shared_ptr(permissive_shared_ptr const& other) noexcept
            : _box(other._box) {

            if (_box) {
                _box->_refs.fetch_add(1, std::memory_order_relaxed);
            }
        }

        permissive_shared_ptr(permissive_shared_ptr&& other) noexcept
            : _box(other._box) {

            other._box = nullptr;
        }

        ~permissive_shared_ptr() noexcept(std::is_nothrow_destructible_v<T>) {
            reset();
        }

        void
        reset() noexcept(std::is_nothrow_destructible_v<T>) {
            if (_box) {
                auto const refs = _box->_refs.fetch_sub(1, std::memory_order_relaxed);
                if (refs == 1) {
                    // It was 1 right before we decremented it. Therefore
                    // we can destroy it now.
                    delete _box->_ptr;
                    delete _box;
                }
                _box = nullptr;
            }
        }

        element_type*
        get() const noexcept {
            return _box ? _box->_ptr : nullptr;
        }

        permissive_shared_ptr<T>&
        operator= (permissive_shared_ptr const& other) noexcept(std::is_nothrow_destructible_v<T>) {
            reset();
            _box = other._box;
            if (_box) {
                _box->_refs.fetch_add(1, std::memory_order_relaxed);
            }
            return *this;
        }

        permissive_shared_ptr<T>&
        operator= (permissive_shared_ptr&& other) noexcept(std::is_nothrow_destructible_v<T>) {
            reset();
            _box = other._box;
            other._box = nullptr;
            return *this;
        }

        T&
        operator* () const noexcept {
            assert(_box);
            return *get();
        }

        T*
        operator-> () const noexcept {
            assert(_box);
            return get();
        }

    private:
        struct box {
            box(T* ptr) noexcept
                : _ptr(ptr)
                , _refs(1) {};

            T* _ptr;
            std::atomic<size_t> _refs;
        };

        box* _box;
    };

    template <typename T, typename... Args>
    inline permissive_shared_ptr<T>
    make_permissive_shared(Args&&...args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        return permissive_shared_ptr<T>(new T(std::forward<Args>(args)...));
    }
}
