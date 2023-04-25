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

            _opts.logfile.put(c);

            if (_to_stderr || _opts.mode == pkg_chk::mode::LIST_BIN_PKGS) {
                std::cerr.put(c);
            }
            else {
                std::cout.put(c);
            }
        }
        return ch;
    }

    std::streamsize
    logger::logger_buf::xsputn(const char_type* s, std::streamsize count) {
        _opts.logfile.write(s, count);

        if (_to_stderr || _opts.mode == pkg_chk::mode::LIST_BIN_PKGS) {
            std::cerr.write(s, count);
        }
        else {
            std::cout.write(s, count);
        }
        return count;
    }

    logger
    fatal_later(pkg_chk::options const& opts) {
        if (!delayed_fatality) {
            delayed_fatality = true;
            std::atexit(exit_for_failure);
        }
        logger l(opts, true);
        l << "** ";
        return l;
    }
}
