#include <algorithm>
#include <cassert>
#include <cerrno>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <signal.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>

#include "iterable.hxx"
#include "nursery.hxx"
#include "scoped_signal_handler.hxx"
#include "signal.hxx"

namespace {
    /** A dummy signal handler, only to prevent signals from being ignored.
     */
    void dummy_handler(int) {}
}

namespace pkgxx {
    namespace detail {
        /** The service is started when scoped_signal_handler is
         * instantiated and there's no server running. It is stopped when
         * the last instance of \ref scoped_signal_handler is
         * destructed. Its purpose is to block all the relevant signals
         * (the union of currently active subscribers in the process) with
         * sigprocmask(2), accept signals with sigwait(2), and broadcast
         * signals to subscribers.
         */
        struct scoped_signal_service {
            scoped_signal_service();
            ~scoped_signal_service();

            /** Obtain the shared instance of scoped_signal_service.
             */
            static std::shared_ptr<scoped_signal_service>
            instance();

            std::future<void>
            subscribe(scoped_signal_handler const& subber);

            std::future<void>
            unsubscribe(scoped_signal_handler const& subber);

        private:
            void
            waiter_main();

            void
            notify_waiter();

        private:
            using mutex_t = std::mutex;
            using lock_t  = std::lock_guard<mutex_t>;

            mutex_t _mtx;
            std::unordered_map<
                std::thread::id,
                std::deque<scoped_signal_handler const*>
                > _subbers; // Invariant: deque's are non-empty
            bool _terminate;
            std::deque<std::promise<void>> _subbers_changed;
            std::thread _waiter;
        };

        scoped_signal_service::scoped_signal_service()
            : _terminate(false) {

            // Before returning from this constructor, we must wait until
            // the waiter thread sets up the signal mask. Otherwise
            // notify_waiter() will end up killing our process.
            auto& prom = _subbers_changed.emplace_back();
            _waiter =
                std::thread(
                    std::bind(&scoped_signal_service::waiter_main, this));
            prom.get_future().wait();
        }

        scoped_signal_service::~scoped_signal_service() {
            {
                lock_t lk(_mtx);
                assert(_subbers.empty());
                _terminate = true;
            }
            notify_waiter();
            _waiter.join();
        }

        std::shared_ptr<scoped_signal_service>
        scoped_signal_service::instance() {
            // Initialization of static local variables is guaranteed to be
            // thread-safe since C++11.
            static std::mutex mtx;
            static std::weak_ptr<scoped_signal_service> wp;

            // We must guard weak_ptr with a mutex because we are
            // mutating it from many threads.
            std::lock_guard<std::mutex> lk(mtx);

            if (auto sp = wp.lock()) {
                return sp;
            }
            else {
                wp = sp = std::make_shared<scoped_signal_service>();
                return sp;
            }
        }

        std::future<void>
        scoped_signal_service::subscribe(scoped_signal_handler const& subber) {
            lock_t lk(_mtx);

            auto const& tid = std::this_thread::get_id();
            _subbers[tid].push_back(&subber);

            auto& prom = _subbers_changed.emplace_back();
            notify_waiter();

            return prom.get_future();
        }

        std::future<void>
        scoped_signal_service::unsubscribe(scoped_signal_handler const& subber) {
            lock_t lk(_mtx);

            auto const& tid = std::this_thread::get_id();

            auto it = _subbers.find(tid);
            assert(it != _subbers.end());

            auto& [_, stack] = *it;
            assert(!stack.empty());
            assert(stack.back() == &subber);
            stack.pop_back();

            if (stack.empty()) {
                _subbers.erase(it);
            }

            auto& prom = _subbers_changed.emplace_back();
            notify_waiter();

            return prom.get_future();
        }

        void
        scoped_signal_service::waiter_main() {
            csigset const saved_procmask = csigset::procmask();
            csigset current_procmask;
            std::unordered_map<int, csigaction> saved_sigacts;

            std::unique_lock<mutex_t> lk(_mtx);
            while (!_terminate) {
                if (!_subbers_changed.empty()) {
                    // Block signals so that we can sigwaitinfo(2) them. We
                    // also block SIGUSR1 for our own purpose, i.e. to
                    // notify this thread via sigqueue(2).
                    current_procmask = saved_procmask;
                    for (auto const& [_, stack]: _subbers) {
                        for (auto const& subber: stack) {
                            for (int const signo: subber->_signals) {
                                current_procmask.insert(signo);
                            }
                        }
                    }
                    current_procmask.insert(SIGUSR1);
                    csigset const prev =
                        csigset::procmask(csigset::setmask, current_procmask);

                    // Blocking signals is not enough. We need to make sure
                    // these signals aren't ignored, otherwise sigwait(2)
                    // may also ignore them. Some kernels actually do this
                    // (e.g. NetBSD and Darwin). Note that this
                    // unconditionally installs our action for SIGUSR1.
                    csigset added;
                    std::set_difference(
                        current_procmask.begin(), current_procmask.end(),
                        prev.begin(), prev.end(),
                        std::inserter(added, added.end()));
                    for (int const signo: added) {
                        assert(!saved_sigacts.count(signo));
                        csigaction sa;
                        sa.handler() = &dummy_handler;
                        saved_sigacts.emplace(signo, sa.install(signo));
                    }

                    // Actions for these signals should now be restored.
                    csigset removed;
                    std::set_difference(
                        prev.begin(), prev.end(),
                        current_procmask.begin(), current_procmask.end(),
                        std::inserter(removed, removed.end()));
                    for (int const signo: removed) {
                        auto const saved_sa = saved_sigacts.find(signo);
                        assert(saved_sa != saved_sigacts.end());
                        saved_sa->second.install(signo);
                        saved_sigacts.erase(saved_sa);
                    }

                    // Now that we have properly updated the signal mask
                    // and actions, we can unblock our subscribers.
                    for (auto& prom: _subbers_changed) {
                        prom.set_value();
                    }
                    _subbers_changed.clear();
                }

                // While waiting for signals we must unlock the mutex,
                // otherwise no other threads can subscribe.
                lk.unlock();
                auto const info = csigwaitinfo(current_procmask);
                lk.lock();

                // We got a signal. But it might be a special one we
                // interrupt the sigwait call above. See comments in
                // notify_waiter().
                if (info->signo == SIGUSR1) {
                    if (is_sigwaitinfo_available() && is_sigqueue_available()) {
                        if (csiginfo_queued* const queued =
                            dynamic_cast<csiginfo_queued*>(info.get());
                            queued &&
                            queued->pid == getpid() &&
                            queued->value.sival_ptr == this) {

                            // It's most likely us that sent this signal.
                            continue;
                        }
                    }
                    else {
                        // Dammit. We must absorb the signal
                        // unconditionally.
                        continue;
                    }
                }

                // The signal turned out to be a genuine one. Broadcast it
                // to our subscribers.
                nursery n;
                for (auto const& [_, subbers]: _subbers) {
                    for (auto const subber: reverse(subbers)) {
                        if (subber->_signals.count(info->signo)) {
                            // We should not be holding the lock of the
                            // mutex here, because the handler may
                            // block. However, actually doing so would lead
                            // to an issue because member variables of this
                            // class might get mutated while it's
                            // unlocked. We are iterating on _subbers right
                            // now, so that is unacceptable.
                            n.start_soon([subber, &info]() {
                                // Maybe we should somehow allow
                                // subscribers to re-raise the signal up
                                // the scope? Is that worth it?
                                subber->_handler(info->signo);
                                // Perhaps subscribers want the full
                                // siginfo?
                            });
                        }
                    }
                }

                // The nursery is destroyed here. We invoke handlers
                // parallelly, and we're sure they all have completed at
                // this point.
            }

            // We are exiting waiter_main(). Restore any sigactions we have
            // changed.
            for (auto const& [signo, sa]: saved_sigacts) {
                sa.install(signo);
            }

            // Then we can restore the signal mask.
            csigset::procmask(csigset::setmask, saved_procmask);
        }

        void
        scoped_signal_service::notify_waiter() {
            // To interrupt our own sigwaitinfo(2) loop, we send SIGUSR1 with a
            // special sigval_t, namely the address of "this". Nothing prevent
            // other threads from intentionally mimicking our internal signal,
            // but it still won't happen accidentally.
            csigqueue(getpid(), SIGUSR1, this);
        }
    }

    scoped_signal_handler::scoped_signal_handler(
        std::initializer_list<int> const& signals,
        std::function<void (int)>&& handler)
        : _signals(signals)
        , _handler(std::move(handler))
          // This is what keeps the service alive.
        , _service(detail::scoped_signal_service::instance()) {

        // We can exit the constructor only after the subscription is
        // confirmed by the waiter thread. Otherwise the scope can miss
        // signals it should be handling.
        _service->subscribe(*this).wait();
    }

    scoped_signal_handler::~scoped_signal_handler() {
        // We can exit the destructor only after the unsubscription is
        // confirmed by the waiter thread. Otherwise a signal arriving
        // right after the unsubscription may cause the waiter to
        // use-after-free.
        _service->unsubscribe(*this).wait();
    }
}
