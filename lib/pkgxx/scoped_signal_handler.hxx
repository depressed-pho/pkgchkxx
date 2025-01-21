#pragma once

#include <functional>
#include <initializer_list>
#include <unordered_set>

namespace pkgxx {
    namespace detail {
        struct scoped_signal_service;
    }

    /** POSIX signal handlers but contained in a scope.
     */
    struct scoped_signal_handler {
        /** Register a handler for a set of signals. Each time a signal
         * arrives, the handler function will be called in a signal-safe
         * context (but in a separate thread). This is achieved using
         * sigprocmask(2) and sigwait(2) so the global signal handlers
         * won't be affected or called.
         *
         * The handler function can do whatever it wants to do. It can
         * allocate memory, block on mutexes, and can even throw
         * exceptions. However, throwing an exception from the handler
         * immediately terminates the entire process via \c
         * std::terminate().
         *
         * If you value portability you should avoid handling \c SIGUSR1
         * using this functionality. On platforms where \c sigwaitinfo(2)
         * or \c sigqueue(2) is missing, \c scoped_signal_handler
         * internally uses \c SIGUSR1 and it will never make it to
         * user-defined handlers.
         */
        scoped_signal_handler(
            std::initializer_list<int> const& signals,
            std::function<void (int)>&& handler);

        /** Destroying the object unregisters the handler.
         */
        ~scoped_signal_handler();

    private:
        friend struct detail::scoped_signal_service;

        std::unordered_set<int> _signals;
        std::function<void (int)> _handler;
        std::shared_ptr<detail::scoped_signal_service> _service;
    };
}
