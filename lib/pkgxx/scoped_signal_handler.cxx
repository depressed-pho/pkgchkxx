#include <cerrno>
#include <exception>
#include <system_error>
#include <unistd.h>
#include <utility>

#include "scoped_signal_handler.hxx"

namespace {
    // A dummy signal handler, only to prevent a signal from being ignored.
    void dummy_handler(int) {}
}

namespace pkgxx {
    scoped_signal_handler::scoped_signal_handler(
        std::initializer_list<int> const& signals,
        std::function<void (int)>&& handler)
        : _handler(std::move(handler))
        , _terminate(false) {

        if (auto const it = signals.begin(); it != signals.end()) {
            _any_signum = *it;
        }
        else {
            // Empty signals is disallowed because we need to send one of
            // the signals to our handler thread to kill it.
            throw std::runtime_error("signals must not be empty");
        }

        // Block the signals so that no existing handlers will catch
        // them. Also we don't want them to terminate the process.
        if (sigemptyset(&_sigset) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigemptyset");
        }
        for (int const signum: signals) {
            if (sigaddset(&_sigset, signum) != 0) {
                throw std::system_error(errno, std::generic_category(), "sigaddset");
            }
        }
        if (sigprocmask(SIG_BLOCK, &_sigset, nullptr) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigprocmask");
        }

        // Blocking signals is not enough. We need to make sure these
        // signals aren't ignored, otherwise sigwait(2) may also ignore
        // them.
        for (int const signum: signals) {
            struct sigaction sa;
            sa.sa_handler = &dummy_handler;
            sa.sa_flags   = 0;
            if (sigemptyset(&sa.sa_mask) != 0) {
                throw std::system_error(errno, std::generic_category(), "sigemptyset");
            }

            struct sigaction saved_sa;
            if (sigaction(signum, &sa, &saved_sa) != 0) {
                throw std::system_error(errno, std::generic_category(), "sigaction");
            }
            _saved_sigacts[signum] = std::move(saved_sa);
        }

        // Now that we have blocked the signals, we can spawn a thread that
        // sigwait(2)s them.
        _thr = std::thread(
                   std::bind(
                       &scoped_signal_handler::thread_main, this));
    }

    scoped_signal_handler::~scoped_signal_handler() {
        // Relaxed store is fine because there are nothing else to
        // synchronize.
        _terminate.store(true, std::memory_order_relaxed);

        // Send a signal to ourselves so that our handler thread notices
        // and exits.
        kill(getpid(), _any_signum);

        // Wait for the exit of thread.
        _thr.join();

        // Restore sigactions we have changed.
        for (auto const& [signum, saved_sa]: _saved_sigacts) {
            if (sigaction(signum, &saved_sa, nullptr) != 0) {
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wterminate"
                // This should never happen in practice. Not worth throwing
                // an exception from dtor.
                throw std::system_error(errno, std::generic_category(), "sigaction");
#  pragma GCC diagnostic pop
            }
        }

        // Now we can unblock the signals.
        if (sigprocmask(SIG_UNBLOCK, &_sigset, nullptr) != 0) {
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wterminate"
            // This should never happen in practice. Not worth throwing an
            // exception from dtor.
            throw std::system_error(errno, std::generic_category(), "sigprocmask");
#  pragma GCC diagnostic pop
        }
    }

    void
    scoped_signal_handler::thread_main() {
        while (true) {
            int sig;
            if (sigwait(&_sigset, &sig) != 0) {
                // This should never happen in practice. Not worth
                // forwarding an exception to the parent.
                throw std::system_error(errno, std::generic_category(), "sigwait");
            }

            if (_terminate.load(std::memory_order_relaxed)) {
                // The signal was sent as a termination request. Don't call
                // the handler.
                break;
            }
            else {
                _handler(sig);
            }
        }
    }
}
