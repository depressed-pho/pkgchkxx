#pragma once

#include <cassert>
#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

namespace pkgxx {
    /** An implementation of structured concurrency:
     * https://vorpus.org/blog/notes-on-structured-concurrency-or-go-statement-considered-harmful/
     */
    struct nursery {
        /** Create a nursery with a given maximum concurrency. It is
         * typically the number of available CPUs.
         */
        nursery(unsigned concurrency
                    = std::max(1u, std::thread::hardware_concurrency()));

        /** Block until all the registered child tasks finish.
         *
         * This is a memory barrier. Whatever memory values children tasks
         * could see before terminating can also be seen by the thread
         * destroying the nursery.
         */
        ~nursery() noexcept(false);

        /** Register a child task to a \ref nursery. The supplied function
         * will run in a separate thread. It is guaranteed to be started
         * before the destructor of nursery returns.
         *
         * \ref nursery does not spawn a separate thread for each child
         * task. Instead it creates a pool of threads and reuses them.
         *
         * If a child task throws an exception, it will be caught by the
         * \ref nursery and rethrown from either its destructor or the next
         * call of \ref start_soon(). No attempts will be made to kill any
         * running children in this case, that is, the rethrowing will not
         * happen until every ongoing task stops running by either
         * finishing or throwing an exception. If more than one child
         * throws an exception, only the first one will be rethrown and
         * others will be discarded.
         *
         * You may create a nested \ref nursery in the task, but the
         * concurrency isn't coordinated with the parent \ref nursery.
         *
         * This is a memory barrier. Whatever memory values the thread
         * calling this function can also be seen by the child task.
         */
        template <typename Function, typename... Args>
        void
        start_soon(Function&& f, Args&&... args) {
            static_assert(std::is_invocable_v<Function&&, Args&&...>);
            lock_t lk(_mtx);

            if (_ex) {
                // We have a pending exception. Delete any pending tasks so
                // that our destructor won't start them.
                _pending_tasks.clear();

                // Clear the recorded exception so that our destructor
                // won't try to rethrow it again.
                auto ex = _ex;
                _ex = {};

                std::rethrow_exception(ex);
            }
            else {
                _pending_tasks.emplace_back(
                    std::forward<Function>(f), std::forward<Args>(args)...);
                start_some();
            }
        }

    private:
        struct task {
            template <typename Function, typename... Args>
            task(Function&& f, Args&&... args)
                : run(
                    std::bind(
                        std::forward<Function>(f), std::forward<Args>(args)...)) {

                static_assert(std::is_invocable_v<Function&&, Args&&...>);
            }

            task() = delete;
            task(task const&) = delete;
            task(task&&) = default;

            std::function<void ()> run;
        };

        struct worker {
            worker(nursery& n)
                : _nursery(n)
                , _terminate(false)
                , _thr(std::bind(&worker::thread_main, this)) {}
            worker() = delete;
            worker(worker const&) = delete;
            worker(worker&&) = default;

            std::thread::id
            get_id() const {
                return _thr.get_id();
            }

            void
            assign(task&& t) {
                {
                    lock_t lk(_mtx);

                    assert(!_task.has_value());
                    _task.emplace(std::move(t));
                }
                _got_request.notify_one();
            }

            void
            async_terminate() {
                {
                    lock_t lk(_mtx);
                    _terminate = true;
                }
                _got_request.notify_one();
            }

            void
            join() {
                _thr.join();
            }

        private:
            void thread_main();

            using mutex_t   = std::mutex;
            using lock_t    = std::lock_guard<mutex_t>;
            using condvar_t = std::condition_variable;

            // These members aren't guarded by a mutex.
            nursery& _nursery;
            mutable mutex_t _mtx;

            // When this has a value it means the worker is busy. Guarded
            // by mtx.
            std::optional<task> _task;

            // Set to true when the nursery wants the worker to
            // die. Guarded by mtx.
            bool _terminate;

            // Signaled when the nursery assigns a task to an idle worker
            // or it asks the worker to die.
            condvar_t _got_request;

            // The thread object isn't guarded by the mutex.
            std::thread _thr;
        };

        // Start some pending tasks as long as we haven't reached the
        // maximum concurrency.
        void
        start_some();

        // Notify the nursery that a task either finished successfully or
        // threw an exception.
        void
        task_finished(std::thread::id worker_id, std::exception_ptr ex = {});

        using mutex_t   = std::recursive_mutex;
        using lock_t    = std::lock_guard<mutex_t>;
        using condvar_t = std::condition_variable_any;

        mutable mutex_t _mtx;
        unsigned _concurrency;

        // The list of tasks that haven't started yet.
        std::deque<task> _pending_tasks;

        // The pool of worker threads.
        std::map<std::thread::id, std::unique_ptr<worker>> _workers;

        // The set of busy worker threads.
        std::set<std::thread::id> _busy_workers;

        // The set of idle worker threads.
        std::set<std::thread::id> _idle_workers;

        // An exception thrown by a task.
        std::exception_ptr _ex;

        // Signaled when a task finishes.
        condvar_t _finished;
    };
}
