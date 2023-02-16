#include <atomic>
#include <cstdlib>

#include "message.hxx"

namespace {
    // Unholy global variable...
    std::atomic<bool> delayed_fatality = false;

    [[noreturn]] void
    exit_for_failure() {
        std::exit(1);
    }
}

namespace pkg_chk {
    logger::logger_buf::int_type
    logger::logger_buf::overflow(int_type ch) {
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            char_type const c = traits_type::to_char_type(ch);

            if (auto const buf = _opts.logfile.rdbuf()) {
                buf->sputc(c);
            }

            if (_to_stderr || _opts.mode == pkg_chk::mode::LIST_BIN_PKGS) {
                if (auto const buf = std::cerr.rdbuf()) {
                    buf->sputc(c);
                }
            }
            else {
                if (auto const buf = std::cout.rdbuf()) {
                    buf->sputc(c);
                }
            }
        }
        return ch;
    }

    std::streamsize
    logger::logger_buf::xsputn(const char_type* s, std::streamsize count) {
        if (auto const buf = _opts.logfile.rdbuf()) {
            buf->sputn(s, count);
        }

        if (_to_stderr || _opts.mode == pkg_chk::mode::LIST_BIN_PKGS) {
            if (auto const buf = std::cerr.rdbuf()) {
                buf->sputn(s, count);
            }
        }
        else {
            if (auto const buf = std::cout.rdbuf()) {
                buf->sputn(s, count);
            }
        }
        return count;
    }

    logger
    fatal_later(pkg_chk::options const& opts) {
        if (!delayed_fatality) {
            delayed_fatality = true;
            std::atexit(exit_for_failure);
        }
        return msg(opts);
    }
}
