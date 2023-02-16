#pragma once

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

#include "options.hxx"

namespace pkg_chk {
    struct logger: public std::ostream {
        logger()
            : std::ostream(nullptr) {}

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
            overflow(int_type ch = traits_type::eof());

            virtual std::streamsize
            xsputn(const char_type* s, std::streamsize count);

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

    inline logger
    verbose(pkg_chk::options const& opts) {
        if (opts.verbose) {
            return logger(opts, true);
        }
        else {
            return logger();
        }
    }

    inline void
    verbose_var(
        pkg_chk::options const& opts,
        std::string_view const& var,
        std::string_view const& value) {

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
    fatal(pkg_chk::options const& opts, Function const& f) {
        logger l(opts, true);
        l << "** ";
        f(l);
        std::exit(1);
    }

    logger
    fatal_later(pkg_chk::options const& opts);
}
