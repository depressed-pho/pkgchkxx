#pragma once

#include <atomic>
#include <functional>
#include <initializer_list>
#include <map>
#include <signal.h>
#include <thread>

namespace pkgxx {
    struct scoped_signal_handler {
        /** Register a handler for a set of signals. Each time a signal
         * arrives, the handler function will be called in a signal-safe
         * context (but in a separate thread). This is achieved using
         * sigprocmask(2) and sigwait(2) so the global signal handlers
         * won't be affected or called. \c signals must not be empty.
         */
        scoped_signal_handler(
            std::initializer_list<int> const& signals,
            std::function<void (int)>&& handler);

        /** Destroying the object unregisters the handler.
         */
        ~scoped_signal_handler();

    private:
        void
        thread_main();

    private:
        std::function<void (int)> _handler;
        std::atomic<bool> _terminate;
        std::map<int, struct sigaction> _saved_sigacts;
        sigset_t _sigset;
        int _any_signum;
        std::thread _thr;
    };
}
