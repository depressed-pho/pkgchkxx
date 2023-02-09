#pragma once

#include <iostream>
#include <string>

#include "options.hxx"

namespace pkg_chk {
    struct logger {
        logger(pkg_chk::options const& opts, bool verbose)
            : _opts(opts)
            , _verbose(verbose) {}

        template <typename T>
        logger&
        operator<< (T val) {
            _opts.logfile << val;

            if (_verbose || _opts.mode == mode::LIST_BIN_PKGS) {
                std::cerr << val;
            }
            else {
                std::cout << val;
            }

            return *this;
        }

    private:
        pkg_chk::options const& _opts;
        bool _verbose;
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
            << "Variable: " << var << " = " << (value.empty() ? "(empty)" : value) << "\n";
    }
}
