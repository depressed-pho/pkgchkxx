#pragma once

#include <iostream>
#include <mutex>
#include <thread>
#include <optional>
#include <type_traits>

#include "options.hxx"

namespace pkg_rr {
    namespace detail {
        inline std::recursive_mutex message_mutex;
    }

    struct msg_logger: public std::ostream {
        msg_logger()
            : std::ostream(nullptr) {}

        msg_logger(std::ostream& out)
            : std::ostream(nullptr)
            , _buf(std::in_place, out) {

            rdbuf(&_buf.value());
        }

        msg_logger(msg_logger&& l)
            : std::ostream(std::move(l))
            , _buf(std::move(l._buf)) {

            if (_buf) {
                rdbuf(&_buf.value());
            }
        }

    private:
        struct msg_buf: public std::streambuf {
            msg_buf(std::ostream& out)
                : _out(out)
                , _state(state::initial) {}

        protected:
            virtual int_type
            overflow(int_type ch = traits_type::eof()) override;

            virtual std::streamsize
            xsputn(const char_type* s, std::streamsize count) override;

        private:
            enum class state {
                initial, // The next output sould follow "RR> "
                newline, // The next output should follow "rr> "
                general
            };

            void
            print_prefix();

            std::ostream& _out;
            state _state;
        };

        std::optional<msg_buf> _buf;
    };

    inline msg_logger
    msg() {
        return msg_logger(std::cout);
    }

    template <typename Function>
    inline void
    atomic_msg(Function&& f) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::recursive_mutex> lk(detail::message_mutex);
        f(std::cout);
    }

    inline msg_logger
    warn() {
        auto out = msg();
        out << "WARNING: ";
        return out;
    }

    template <typename Function>
    void
    atomic_warn(Function&& f) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::recursive_mutex> lk(detail::message_mutex);
        auto l = warn();
        f(l);
    }

    template <typename Function>
    inline void
    atomic_error(Function&& f) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::recursive_mutex> lk(detail::message_mutex);
        std::cout << "*** ";
        f(std::cout);
    }

    inline msg_logger
    verbose(pkg_rr::options const& opts, unsigned level = 1) {
        if (opts.verbose >= level) {
            return msg_logger(std::cout);
        }
        else {
            return msg_logger();
        }
    }

    template <typename Function>
    inline void
    atomic_verbose(pkg_rr::options const& opts, Function&& f, unsigned level = 1) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::recursive_mutex> lk(detail::message_mutex);
        auto out = verbose(opts, level);
        f(out);
    }

    inline void
    verbose_var(
        pkg_rr::options const& opts,
        std::string_view const& var,
        std::string_view const& value,
        unsigned level = 2) {

        std::lock_guard<std::recursive_mutex> lk(detail::message_mutex);
        verbose(opts, level)
            << "Variable: " << var << " = " << (value.empty() ? "(empty)" : value) << std::endl;
    }

    template <typename Rep, typename Period>
    void vsleep(
        pkg_rr::options const& opts,
        const std::chrono::duration<Rep, Period>& duration,
        unsigned level = 2) {

        if (opts.verbose >= level) {
            std::this_thread::sleep_for(duration);
        }
    }

    template <typename Function>
    [[noreturn]] inline void
    fatal(Function&& f) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::recursive_mutex> lk(detail::message_mutex);
        std::cout << "*** ";
        f(std::cout);
        std::exit(1);
    }
}
