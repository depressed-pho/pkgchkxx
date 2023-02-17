#pragma once

#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace pkg_chk {
    /* An implementation of structured concurrency:
     * https://vorpus.org/blog/notes-on-structured-concurrency-or-go-statement-considered-harmful/
     */
    struct nursery {
        /* Create a nursery with a given maximum concurrency. It is
         * typically the number of available CPUs.
         */
        nursery(unsigned int concurrency = std::thread::hardware_concurrency());

        /* Block until all the registered child tasks finish.
         *
         * This is a memory barrier. Whatever memory values children tasks
         * could see before terminating can also be seen by the thread
         * destroying the nursery.
         */
        ~nursery() noexcept(false);

        /* Register a child task to a nursery. The supplied function will
         * run in a separate thread. It is guaranteed to be started before
         * the destructor of nursery returns.
         *
         * If a child task throws an exception, it will caught by the
         * nursery and rethrown from its destructor. No attempts will be
         * made to kill any running children in this case, that is, the
         * rethrowing will not happen until every child stops running by
         * either finishing or throwing an exception. If more than one
         * child throws an exception, only the first one will be rethrown
         * and others will be discarded.
         *
         * This is a memory barrier. Whatever memory values the thread
         * calling this function can also be seen by the child task.
         */
        template <typename Function, typename... Args>
        void
        start_soon(Function&& f, Args&&... args) {
            static_assert(std::is_invocable_v<Function&&, Args&&...>);
            lock_t lk(_mtx);

            _pending_children.emplace_back(
                new child(std::forward(f), std::forward(args...)));
            start_some();
        }

    private:
        // Start some pending children as long as we haven't reached the
        // maximum concurrency.
        void
        start_some();

    private:
        struct child {
            template <typename Function, typename... Args>
            child(Function&& f, Args&&... args)
                : run(std::bind(std::forward(f), std::forward(args...))) {

                static_assert(std::is_invocable_v<Function&&, Args&&...>);
            }

            std::function<void ()> run;
        };

        using mutex_t   = std::recursive_mutex;
        using lock_t    = std::lock_guard<mutex_t>;
        using condvar_t = std::condition_variable_any;
        unsigned int _concurrency;
        mutex_t _mtx;

        // The list of children that haven't started yet.
        std::deque<std::shared_ptr<child>> _pending_children;

        // The map of children that have started but haven't finished.
        std::map<std::thread::id, std::shared_ptr<child>> _running_children;

        // An exception thrown by a child.
        std::exception_ptr _ex;

        // Signaled when a child finishes running.
        condvar_t _finished;
    };
}
