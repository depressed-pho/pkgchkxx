#include "message.hxx"

namespace pkg_chk {
    logger::logger_buf::int_type
    logger::logger_buf::overflow(int_type ch) {
        if (traits_type::not_eof(ch)) {
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

    void
    verbose_var(
        pkg_chk::options const& opts,
        std::string const& var,
        std::string const& value) {

        verbose(opts)
            << "Variable: " << var << " = " << (value.empty() ? "(empty)" : value) << std::endl;
    }
}
