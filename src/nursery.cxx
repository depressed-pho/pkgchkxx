#include "nursery.hxx"

using namespace std::placeholders;

namespace pkg_chk {
    nursery::nursery(unsigned int concurrency)
        : _concurrency(concurrency) {}

    nursery::~nursery() noexcept(false) {
        std::unique_lock<mutex_t> lk(_mtx);

        while (true) {
            if (!_ex) {
                start_some();
            }

            if (!_pending_tasks.empty() && !_ex) {
                // We still have pending tasks, which means we have reached
                // the maximum concurrency. In this case we should wait
                // until some tasks finish so that we can start more.
                _finished.wait(lk);
            }
            else if (!_busy_workers.empty()) {
                // We have no pending children anymore but some tasks are
                // still running. Wait until they all finish.
                _finished.wait(lk);
            }
            else {
                // No pending nor running tasks. Terminate all the
                // workers. But since workers will call our
                // task_finished(), which locks our mutex, we must unlock
                // it to avoid deadlock.
                lk.unlock();
                for (auto& w: _workers) {
                    w.second->async_terminate();
                }
                for (auto& w: _workers) {
                    w.second->join();
                }
                // No need to worry about this lock() being skipped when
                // join() throws.
                lk.lock();
                break;
            }
        }

        if (_ex) {
            std::rethrow_exception(_ex);
        }
    }

    void
    nursery::start_some() {
        lock_t lk(_mtx);

        auto const pop_task =
            [this]() {
                task t = std::move(_pending_tasks.front());
                _pending_tasks.pop_front();
                return t;
            };

        while (!_pending_tasks.empty()) {
            if (auto it = _idle_workers.begin(); it != _idle_workers.end()) {
                // We have an idle worker. Assign a task to it.
                _workers.at(*it)->assign(pop_task());
                _busy_workers.insert(*it);
                _idle_workers.erase(it);
            }
            else if (_workers.size() < _concurrency) {
                // We have no idle workers but we have a room for spawning
                // more.
                auto w = std::make_unique<worker>(*this);
                auto const worker_id = w->get_id();

                w->assign(pop_task());
                _workers.insert_or_assign(worker_id, std::move(w));
                _busy_workers.insert(worker_id);
            }
            else {
                // We have reached the maximum concurrency.
                break;
            }
        }
    }

    void
    nursery::task_finished(std::thread::id worker_id, std::exception_ptr ex) {
        {
            lock_t lk(_mtx);

            if (ex && !_ex) {
                _ex = ex;
            }
            _busy_workers.erase(worker_id);
            _idle_workers.insert(worker_id);
        }
        _finished.notify_one();
    }

    void
    nursery::worker::thread_main() {
        std::unique_lock<mutex_t> lk(_mtx);

        while (true) {
            // Sleep until we get a task or a termination request. We
            // should not terminate as long as we have a task.
            std::optional<task> t = std::move(_task);
            _task.reset();
            if (t) {
                // fall through
            }
            else if (_terminate) {
                break;
            }
            else {
                _got_request.wait(lk);
                continue;
            }

            // We got a task. Run it now, but don't lock the mutex while
            // doing that. Otherwise the nursery can't even post a
            // terminate request to us.
            lk.unlock();
            try {
                t->run();
                _nursery.task_finished(get_id());
            }
            catch (...) {
                _nursery.task_finished(get_id(), std::current_exception());
            }
            lk.lock();
        }
    }
}
