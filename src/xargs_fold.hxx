#pragma once

#include "config.h"

#include <algorithm>
#include <exception>
#include <cassert>
#include <deque>
#include <istream>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <type_traits>
#include <vector>

#include "harness.hxx"

namespace pkg_chk {
    namespace detail {
        template <typename Parse>
        struct xargs_nursery {
        private:
            using mutex_t   = std::recursive_mutex;
            using lock_t    = std::lock_guard<mutex_t>;
            using condvar_t = std::condition_variable_any;

        public:
            struct split_sink {
                split_sink(xargs_nursery& parent)
                    : _parent(parent)
                    , _lk(parent._mtx)
                    , _next_child(0) {}

                ~split_sink() {
                    _parent.split_done();
                }

                void
                push_back(std::string const& arg) {
                    _parent._harnesses[_next_child]->cin() << arg << '\0';
                    _next_child = (_next_child + 1) % _parent._harnesses.size();
                }

            private:
                // We can lock the parent during the entire lifetime of
                // split_sink because readers (i.e. parser threads) only
                // needs to lock it at their very end.
                xargs_nursery& _parent;
                xargs_nursery::lock_t _lk;
                std::vector<harness>::size_type _next_child;
            };

            static_assert(std::is_invocable_v<Parse, std::istream&>);
            using result_type = std::invoke_result_t<Parse, std::istream&>;
            static_assert(std::is_default_constructible_v<result_type>);
            // result_type must also form a commutative monoid under its
            // default constructor and operator+=.

            xargs_nursery(std::vector<std::string> const& cmd,
                          Parse&& parse,
                          unsigned int concurrency) {
                lock_t lk(_mtx);

                std::vector<std::string> argv = {CFG_XARGS, "-0"};
                argv.insert(argv.end(), cmd.begin(), cmd.end());

                for (unsigned int i = 0; i < concurrency; i++) {
                    auto xargs = std::make_shared<harness>(CFG_XARGS, argv);
                    _harnesses.push_back(xargs);

                    auto parser = std::make_unique<std::thread>(
                        [this, &parse, xargs]() {
                            try {
                                // Don't lock the mutex while parsing the
                                // stdout of the commands. That would
                                // prevent the parallelisation which is the
                                // whole point of this entire machinery.
                                auto result = parse(xargs->cout());

                                lock_t lk_(_mtx);
                                _running_parsers.erase(std::this_thread::get_id());
                                _result += std::move(result);
                            }
                            catch (...) {
                                lock_t lk_(_mtx);
                                _running_parsers.erase(std::this_thread::get_id());
                                if (!_ex) {
                                    _ex = std::current_exception();
                                }
                            }
                            _finished.notify_one();
                        });
                    _running_parsers.insert(parser->get_id());
                    _parsers.push_back(std::move(parser));
                }
            }

            virtual ~xargs_nursery() {
                for (auto& p: _parsers) {
                    p->join();
                }
            }

            // This method must not be called twice.
            split_sink
            sink() {
                return split_sink(*this);
            }

            // This method must not be called twice.
            result_type
            await() {
                std::unique_lock<mutex_t> lk(_mtx);

                while (!_running_parsers.empty()) {
                    _finished.wait(lk);
                }

                return std::move(_result);
            }

        private:
            void
            split_done() {
                lock_t lk(_mtx);

                for (auto& child: _harnesses) {
                    child->cin().close();
                }
            }

            mutex_t _mtx;
            std::vector<std::shared_ptr<harness>> _harnesses;
            std::vector<std::unique_ptr<std::thread>> _parsers;
            std::set<std::thread::id> _running_parsers;
            result_type _result;
            std::exception_ptr _ex;
            condvar_t _finished; // Signaled when a parser finishes running.
        };
    }

    template <typename Split, typename Parse>
    typename detail::xargs_nursery<Parse>::result_type
    xargs_fold(std::vector<std::string> const& cmd,
               Split&& split,
               Parse&& parse,
               unsigned int concurrency = std::max(1u, std::thread::hardware_concurrency())) {

        static_assert(
            std::is_invocable_v<
                Split,
                typename detail::xargs_nursery<Parse>::split_sink&&>);

        assert(concurrency > 0);
        auto nursery = detail::xargs_nursery(cmd, parse, concurrency);

        std::exception_ptr ex;
        auto splitter = std::thread(
            [&]() {
                try {
                    split(nursery.sink());
                }
                catch (...) {
                    ex = std::current_exception();
                }
            });

        auto result = nursery.await();
        splitter.join();
        if (ex) {
            std::rethrow_exception(ex);
        }
        return result;
    }
}
