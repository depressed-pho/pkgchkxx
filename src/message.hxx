#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

#include "options.hxx"

namespace pkg_chk {
    struct logger: public std::ostream {
        logger(pkg_chk::options const& opts, bool to_stderr)
            : std::ostream(nullptr)
            , _buf(opts, to_stderr) {

            rdbuf(&_buf);
        }

        logger(logger&& l)
            : std::ostream(std::move(l))
            , _buf(std::move(l._buf)) {}

        virtual ~logger() {}

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

        logger_buf _buf;
    };

    inline logger
    msg(pkg_chk::options const& opts) {
        return logger(opts, false);
    }

    inline logger
    verbose(pkg_chk::options const& opts) {
        return logger(opts, true);
    }

    inline void
    verbose_var(
        pkg_chk::options const& opts,
        std::string const& var,
        std::string const& value) {

        verbose(opts)
            << "Variable: " << var << " = " << (value.empty() ? "(empty)" : value) << std::endl;
    }

    template <typename Function>
    [[noreturn]] inline void
    fatal(pkg_chk::options const& opts, Function const& f) {
        logger l(opts, true);
        f(l);
        std::exit(1);
    }
}
