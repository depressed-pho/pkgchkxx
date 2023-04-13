#pragma once

#include <iostream>
#include <mutex>
#include <optional>
#include <type_traits>

#include "options.hxx"

namespace pkg_rr {
    namespace detail {
        inline std::mutex message_mutex;
    }

    struct msg_logger: public std::ostream {
        msg_logger()
            : std::ostream(nullptr) {}

        msg_logger(std::ostream& out)
            : std::ostream(nullptr)
            , _buf(std::in_place, out) {

            rdbuf(&_buf.value());
            *this << "RR> ";
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
                , _cont(false) {}

        protected:
            virtual int_type
            overflow(int_type ch = traits_type::eof()) override;

            virtual std::streamsize
            xsputn(const char_type* s, std::streamsize count) override;

        private:
            std::ostream& _out;
            bool _cont; // The next output should follow "rr> "
        };

        std::optional<msg_buf> _buf;
    };

    inline msg_logger
    msg() {
        return msg_logger(std::cout);
    }

    inline msg_logger
    verbose(pkg_rr::options const& opts) {
        if (opts.verbose) {
            return msg_logger(std::cout);
        }
        else {
            return msg_logger();
        }
    }

    inline void
    verbose_var(
        pkg_rr::options const& opts,
        std::string_view const& var,
        std::string_view const& value) {

        std::lock_guard<std::mutex> lk(detail::message_mutex);
        verbose(opts)
            << "Variable: " << var << " = " << (value.empty() ? "(empty)" : value) << std::endl;
    }

    template <typename Function>
    [[noreturn]] inline void
    fatal(Function&& f) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::mutex> lk(detail::message_mutex);
        std::cerr << "*** ";
        f(std::cerr);
        std::exit(1);
    }
}
