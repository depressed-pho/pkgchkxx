#pragma once

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <mutex>
#include <type_traits>

#include "options.hxx"

namespace pkg_chk {
    namespace detail {
        inline std::mutex message_mutex;
    }

    struct logger: public std::ostream {
        logger()
            : std::ostream(nullptr) {}

        logger(logger const&) = delete;

        logger(pkg_chk::options const& opts, bool to_stderr)
            : std::ostream(nullptr)
            , _buf(std::in_place, opts, to_stderr) {

            rdbuf(&_buf.value());
        }

        logger(logger&& l)
            : std::ostream(std::move(l))
            , _buf(std::move(l._buf)) {

            if (_buf) {
                rdbuf(&_buf.value());
            }
        }

    private:
        struct logger_buf: public std::streambuf {
            logger_buf(pkg_chk::options const& opts, bool to_stderr)
                : _opts(opts)
                , _to_stderr(to_stderr) {}

        protected:
            virtual int_type
            overflow(int_type ch = traits_type::eof()) override;

            virtual std::streamsize
            xsputn(const char_type* s, std::streamsize count) override;

        private:
            pkg_chk::options const& _opts;
            bool _to_stderr;
        };

        std::optional<logger_buf> _buf;
    };

    inline logger
    msg(pkg_chk::options const& opts) {
        return logger(opts, false);
    }

    template <typename Function>
    void
    atomic_msg(pkg_chk::options const& opts, Function&& f) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::mutex> lk(detail::message_mutex);
        auto l = msg(opts);
        f(l);
    }

    inline logger
    warn(pkg_chk::options const& opts) {
        logger l(opts, false);
        l << "WARNING: ";
        return l;
    }

    template <typename Function>
    void
    atomic_warn(pkg_chk::options const& opts, Function&& f) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::mutex> lk(detail::message_mutex);
        auto l = warn(opts);
        f(l);
    }

    inline logger
    verbose(pkg_chk::options const& opts) {
        if (opts.verbose) {
            return logger(opts, true);
        }
        else {
            return logger();
        }
    }

    template <typename Function>
    void
    atomic_verbose(pkg_chk::options const& opts, Function&& f) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::mutex> lk(detail::message_mutex);
        auto l = verbose(opts);
        f(l);
    }

    inline void
    verbose_var(
        pkg_chk::options const& opts,
        std::string_view const& var,
        std::string_view const& value) {

        std::lock_guard<std::mutex> lk(detail::message_mutex);
        verbose(opts)
            << "Variable: " << var << " = " << (value.empty() ? "(empty)" : value) << std::endl;
    }

    inline void
    verbose_var(
        pkg_chk::options const& opts,
        std::string_view const& var,
        std::string const& value) {

        verbose_var(opts, var, static_cast<std::string_view>(value));
    }

    template <typename Function>
    [[noreturn]] inline void
    fatal(pkg_chk::options const& opts, Function&& f) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::mutex> lk(detail::message_mutex);
        logger l(opts, true);
        l << "** ";
        f(static_cast<std::ostream&>(l));
        std::exit(1);
    }

    logger
    fatal_later(pkg_chk::options const& opts);

    template <typename Function>
    void
    fatal_later(pkg_chk::options const& opts, Function&& f) {
        static_assert(std::is_invocable_v<Function&&, std::ostream&>);

        std::lock_guard<std::mutex> lk(detail::message_mutex);
        auto l = fatal_later(opts);
        f(l);
    }
}
