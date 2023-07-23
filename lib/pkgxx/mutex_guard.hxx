#pragma once

#include <mutex>
#include <utility>

namespace pkgxx {
    /** An object-guarding container resembling std::sync::Mutex from
     * Rust. Unlike C++11 std::atomic, the contained type T doesn't need to
     * be TriviallyCopyable. It doesn't even need to be Copyable or Movable
     * unless the instance of \c guarded is to be copied or moved.
     */
    template <typename T>
    class guarded {
    public:
        /** A smart-pointer-like object containing a reference to some type
         * \c T. The mutex is locked when the value of \ref mutex_guard is
         * obtained, and it's unlocked when the value goes out of scope.
         */
        class mutex_guard {
            friend class guarded;
            mutex_guard(guarded<T>& cell)
                : _cell(&cell)
                , _lk(cell._mtx) {}

        public:
            /** Obtain a mutable reference to the guarded value. */
            T&
            operator* () const {
                return _cell->_val;
            }

            /** Obtain a mutable pointer to the guarded value. */
            T*
            operator-> () const {
                return &(_cell->_val);
            }

        private:
            guarded<T>* _cell;
            std::lock_guard<std::mutex> _lk;
        };

        /** In-place construction of type T inside guarded<T>.
         */
        template <typename... Args>
        guarded(Args&&... args)
            : _val(std::forward<Args>(args)...) {}

        /** Copy-constructing \c guarded<T> requires \c T to be
         * Copyable. */
        guarded(guarded const& from)
            : _val(from._val) {}

        /** Move-constructing \c guarded<T> requires \c T to be Movable. */
        guarded(guarded&& from)
            : _val(std::move(from._val)) {}

        /** Return an RAII guard that can dereference to the contained
         * value of type T.
         */
        mutex_guard
        lock() {
            return mutex_guard(*this);
        }

    private:
        mutable std::mutex _mtx;
        T _val;
    };
}
