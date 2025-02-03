#pragma once

#include <functional>
#include <initializer_list>
#include <type_traits>
#include <unordered_set>

#include <pkgxx/signal.hxx>

namespace pkgxx {
    namespace detail {
        struct scoped_signal_service;
    }

    /** POSIX signal handlers but contained in a scope.
     */
    struct scoped_signal_handler {
        /** A type of function that, when invoked, passes the current
         * signal down the scopes and ultimately to the process itself.
         */
        using resender_type = std::function<void ()>;

        using handler_type = std::function<void (csiginfo_base const&, resender_type const&)>;

        /** Default-constructed handlers don't actually handle anything.
         */
        scoped_signal_handler();

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
        template <typename Fn>
        scoped_signal_handler(
            std::initializer_list<int> const& signals,
            Fn&& handler)
            : scoped_signal_handler(
                0, signals, make_handler(std::forward<Fn>(handler))) {}

        scoped_signal_handler(scoped_signal_handler const& other);
        scoped_signal_handler(scoped_signal_handler&& other);

        /** Destroying the object unregisters the handler.
         */
        ~scoped_signal_handler();

        scoped_signal_handler&
        operator= (scoped_signal_handler const& other);

        scoped_signal_handler&
        operator= (scoped_signal_handler&& other);

    private:
        scoped_signal_handler(
            int, // a dummy parameter to avoid overload conflicts
            std::initializer_list<int> const& signals,
            handler_type&& handler);

        template <typename Fn>
        handler_type
        make_handler(Fn&& handler) {
            if constexpr (std::is_invocable_v<Fn>) {
                // []() {...}
                //   - takes nothing
                return [handler = std::forward<Fn>(handler)](auto const&, auto const&) {
                    handler();
                };
            }
            else if constexpr (std::is_invocable_v<Fn, int>) {
                // [](int) {...}
                //   - only takes a signal number
                return [handler = std::forward<Fn>(handler)](auto const& si, auto const&) {
                    handler(si.signo());
                };
            }
            else if constexpr (std::is_invocable_v<Fn, csiginfo_base const&>) {
                // [](csiginfo_base const&) { ... }
                //   - only takes a siginfo
                return [handler = std::forward<Fn>(handler)](auto const& si, auto const&) {
                    handler(si);
                };
            }
            else if constexpr (std::is_invocable_v<Fn, int, resender_type const&>) {
                // [](int, resender_type const&) {...}
                //   - takes a signal number and a resender
                return [handler = std::forward<Fn>(handler)](auto const& si, auto const& resender) {
                    handler(si.signo(), resender);
                };
            }
            else {
                static_assert(std::is_invocable_v<Fn, csiginfo_base const&, resender_type const&>);
                // [](csiginfo_base const&, resender_type const&) { ... }
                //   - takes a siginfo and a resender
                return handler_type(std::forward<Fn>(handler));
            }
        }

    private:
        friend struct detail::scoped_signal_service;

        std::unordered_set<int> _signals;
        handler_type _handler;
        std::shared_ptr<detail::scoped_signal_service> _service;
    };
}
