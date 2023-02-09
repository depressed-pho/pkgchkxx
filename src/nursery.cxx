#include "nursery.hxx"

namespace pkg_chk {
    nursery::nursery(unsigned int concurrency)
        : _concurrency(concurrency) {}

    nursery::~nursery() noexcept(false) {
        std::unique_lock<mutex_t> lk(_mtx);

        while (true) {
            start_some();

            if (!_pending_children.empty()) {
                // We still have pending children, which means we have
                // reached the maximum concurrency. In this case we should
                // wait until some children finishes so that we can start
                // more.
                _finished.wait(lk);
            }
            else if (!_running_children.empty()) {
                // We have no pending children anymore but some children
                // are still running. Wait until they all stop.
                _finished.wait(lk);
            }
            else {
                // No pending nor running children. Everything's done.
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

        while (!_pending_children.empty() &&
               _running_children.size() < _concurrency) {

            std::shared_ptr<child> childp = std::move(_pending_children.front());
            _pending_children.pop_front();

            auto thr = std::thread(
                [this, childp]() {
                    try {
                        childp->run();
                    }
                    catch (...) {
                        lock_t lk(_mtx);
                        if (!_ex) {
                            _ex = std::current_exception();
                        }
                    }

                    // Now that the child has finished (or died), we want
                    // to remove it from running_children. But before doing
                    // that we have to acquire the lock.
                    {
                        lock_t lk(_mtx);
                        _running_children.erase(std::this_thread::get_id());
                    }

                    // Signal the caller of the destructor so that it can
                    // spawn more children.
                    _finished.notify_one();
                });
            _running_children.emplace(thr.get_id(), std::move(childp));
        }
    }
}
