#pragma once

#include <mutex>

namespace pkg_chk {
    /** An object-guarding container resembling std::sync::Mutex from
     * Rust. Unlike C++11 std::atomic, the contained type T doesn't need to
     * be TriviallyCopyable. It doesn't even need to be Copyable or
     * Movable.
     */
    template <typename T>
    class guarded {
    public:
        class mutex_guard {
        public:
            mutex_guard(guarded<T>& cell)
                : _cell(&cell)
                , _lk(cell._mtx) {}

            T&
            operator* () const {
                return _cell->_val;
            }

            T*
            operator-> () const {
                return &(_cell->_val);
            }

        private:
            guarded<T>* _cell;
            std::lock_guard<typename guarded<T>::mutex_t> _lk;
        };

        /** In-place construction of type T inside guarded<T>.
         */
        template <typename... Args>
        guarded(Args&&... args)
            : _val(std::forward<Args>(args)...) {}

        /** Return an RAII guard that can dereference to the contained
         * value of type T.
         */
        mutex_guard
        lock() {
            return mutex_guard(*this);
        }

    private:
        using mutex_t = std::mutex;

        mutable mutex_t _mtx;
        T _val;
    };
}
