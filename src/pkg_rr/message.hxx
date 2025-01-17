#pragma once

#include <iostream>
#include <thread>
#include <optional>
#include <type_traits>

#include "options.hxx"

namespace pkg_rr {
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
    msg(std::ostream& out = std::cerr) {
        return msg_logger(out);
    }

    inline msg_logger
    warn(std::ostream& out = std::cerr) {
        auto out_ = msg(out);
        out_ << "WARNING: ";
        return out_;
    }

    inline msg_logger
    error(std::ostream& out = std::cerr) {
        auto out_ = msg(out);
        out_ << "*** ";
        return out_;
    }

    template <typename Function>
    [[noreturn]] inline void
    fatal(Function&& f, std::ostream& out = std::cerr) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        auto out_ = error(out);
        f(out_);
        std::exit(1);
    }

    inline msg_logger
    verbose(pkg_rr::options const& opts, unsigned level = 1, std::ostream& out = std::cerr) {
        if (opts.verbose >= level) {
            return msg_logger(out);
        }
        else {
            return msg_logger();
        }
    }

    inline void
    verbose_var(
        pkg_rr::options const& opts,
        std::string_view const& var,
        std::string_view const& value,
        unsigned level = 2,
        std::ostream& out = std::cerr) {

        verbose(opts, level, out)
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
}
