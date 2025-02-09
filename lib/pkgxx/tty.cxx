#include "config.h"

#if defined(HAVE_SYS_IOCTL_H)
#  include <sys/ioctl.h>
#endif
#include <system_error>
#include <type_traits>
#include <unistd.h>

#include "always_false_v.hxx"
#include "environment.hxx"
#include "tty.hxx"

namespace pkgxx {
    bool
    cisatty([[maybe_unused]] int const fd) {
#if defined(HAVE_ISATTY)
        return isatty(fd);
#else
        return false;
#endif
    }

    std::optional<dimension<std::size_t>>
    ttystream::size() const {
        if (!fd()) {
            return std::nullopt;
        }

#if defined(HAVE_IOCTL) && defined(TIOCGWINSZ) && defined(HAVE_STRUCT_WINSIZE)
        struct winsize ws;
        if (ioctl(*fd(), TIOCGWINSZ, &ws) != 0) {
            throw std::system_error(errno, std::generic_category(), "ioctl(TIOCGWINSZ)");
        }
        return dimension<std::size_t> { ws.ws_col, ws.ws_row };

#else
        // Dangit, this platform doesn't have TIOCGWINSZ. It's probably
        // Illumos.
        //
        // We could open the controlling terminal device, temporarily turn
        // off ICANON and ECHO, move the cursor to the rightmost column,
        // request the cursor position to tty, read a response from tty,
        // and then parse it. But that complicates matters a lot, like damn
        // fucking a LOT. We can't bear with the complexity it induces in
        // this already suboptimal case. No operating systems relevant
        // today lack TIOCGWINSZ after all.
        return std::nullopt;
#endif
    }

    bool
    ttystream::default_use_colour() {
        // See https://no-color.org/
        return !cgetenv("NO_COLOR").has_value();
    }

    void
    ttystream::apply_style(tty::style const& sty) {
        *this << "\x1B[0";
        if (sty.foreground) {
            auto const& [i, c] = *sty.foreground;
            *this << ';'
                  << 30 + static_cast<int>(i) + static_cast<int>(c);
        }
        if (sty.background) {
            auto const& [i, c] = *sty.background;
            *this << ';'
                  << 40 + static_cast<int>(i) + static_cast<int>(c);
        }
        if (sty.boldness) {
            *this << ';'
                  << static_cast<int>(*sty.boldness);
        }
        if (sty.font) {
            *this << ';'
                  << static_cast<int>(*sty.font);
        }
        if (sty.underline) {
            *this << ';'
                  << static_cast<int>(*sty.underline);
        }
        *this << 'm';
    }

    std::optional<pkgxx::dimension<std::size_t>>
    maybe_ttystreambuf::term_size() const {
        return std::visit(
            [](auto&& arg) -> decltype(term_size()) {
                using Arg = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<Arg, ttystream>) {
                    return arg.size();
                }
                else if constexpr (std::is_same_v<Arg, fdostream>) {
                    return std::nullopt;
                }
                else {
                    static_assert(always_false_v<Arg>);
                }
            }, _out);
    }

    void
    maybe_ttystreambuf::push_style(
        pkgxx::tty::style const& sty,
        pkgxx::ttystream_base::how how_) {

        return std::visit(
            [&](auto&& arg) {
                using Arg = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<Arg, ttystream>) {
                    arg.push_style(sty, how_);
                }
                else if constexpr (std::is_same_v<Arg, fdostream>) {
                    // Do nothing
                }
                else {
                    static_assert(always_false_v<Arg>);
                }
            }, _out);
    }

    void
    maybe_ttystreambuf::pop_style() {
        return std::visit(
            [](auto&& arg) {
                using Arg = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<Arg, ttystream>) {
                    arg.pop_style();
                }
                else if constexpr (std::is_same_v<Arg, fdostream>) {
                    // Do nothing
                }
                else {
                    static_assert(always_false_v<Arg>);
                }
            }, _out);
    }

    int
    maybe_ttystreambuf::sync() {
        return std::visit(
            [](auto&& arg) {
                arg.flush();
                return arg.bad() ? -1 : 0;
            }, _out);
    }

    maybe_ttystreambuf::int_type
    maybe_ttystreambuf::overflow(int_type ch) {
        return std::visit(
            [ch](auto&& arg) {
                if (!traits_type::eq_int_type(ch, traits_type::eof())) {
                    arg.put(traits_type::to_char_type(ch));
                }
                return ch;
            }, _out);
    }

    std::streamsize
    maybe_ttystreambuf::xsputn(const char_type* s, std::streamsize count) {
        return std::visit(
            [s, count](auto&& arg) {
                arg.write(s, count);
                return count;
            }, _out);
    }

    maybe_tty_syncbuf::~maybe_tty_syncbuf() {
        auto _lk = _out.lock();
        for (auto const& cmd: _cmds) {
            std::visit(
                [this](auto&& arg) {
                    using Arg = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<Arg, sync_cmd>) {
                        _out.pubsync();
                    }
                    else if constexpr (std::is_same_v<Arg, write_cmd>) {
                        _out.sputn(arg.data(), static_cast<std::streamsize>(arg.size()));
                    }
                    else if constexpr (std::is_same_v<Arg, push_style_cmd>) {
                        _out.push_style(arg, arg.how_);
                    }
                    else if constexpr (std::is_same_v<Arg, pop_style_cmd>) {
                        _out.pop_style();
                    }
                    else {
                        static_assert(always_false_v<Arg>);
                    }
                }, cmd);
        }
    }

    [[noreturn]] std::lock_guard<std::mutex>
    maybe_tty_syncbuf::lock() {
        assert(0 && "It's a logical error to lock a maybe_tty_syncbuf");
        std::terminate();
    }

    std::optional<pkgxx::dimension<std::size_t>>
    maybe_tty_syncbuf::term_size() const {
        auto _lk = _out.lock();
        return _out.term_size();
    }

    void
    maybe_tty_syncbuf::push_style(
        pkgxx::tty::style const& sty,
        ttystream_base::how how_) {

        _cmds.emplace_back(push_style_cmd(sty, how_));
    }

    void
    maybe_tty_syncbuf::pop_style() {
        _cmds.emplace_back(pop_style_cmd {});
    }

    int
    maybe_tty_syncbuf::sync() {
        _cmds.emplace_back(sync_cmd {});
        return 0;
    }

    maybe_tty_syncbuf::int_type
    maybe_tty_syncbuf::overflow(int_type ch) {
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            auto const c = traits_type::to_char_type(ch);

            if (auto it = _cmds.rbegin(); it != _cmds.rend()) {
                if (auto writep = std::get_if<write_cmd>(&*it); writep) {
                    writep->push_back(c);
                    return ch;
                }
            }
            _cmds.emplace_back(write_cmd(1, c));
        }
        return ch;
    }

    std::streamsize
    maybe_tty_syncbuf::xsputn(const char_type* s, std::streamsize count) {
        if (auto it = _cmds.rbegin(); it != _cmds.rend()) {
            if (auto writep = std::get_if<write_cmd>(&*it); writep) {
                writep->append(s, static_cast<write_cmd::size_type>(count));
                return count;
            }
        }
        _cmds.emplace_back(write_cmd(s, static_cast<write_cmd::size_type>(count)));
        return count;
    }

    std::optional<dimension<std::size_t>>
    maybe_tty_osyncstream::size() const {
        return _buf ? _buf->term_size() : std::nullopt;
    }

    void
    maybe_tty_osyncstream::push_style(tty::style const& sty, how how_) {
        if (_buf) {
            _buf->push_style(sty, how_);
        }
    }

    void
    maybe_tty_osyncstream::pop_style() {
        if (_buf) {
            _buf->pop_style();
        }
    }

    namespace tty {
        namespace detail {
            ttystream_base&
            operator<< (ttystream_base& tty, move_to const& m) {
                if (m.y) {
                    tty << "\x1B[" << *m.y + 1 << ';' << m.x + 1 << 'H';
                }
                else if (m.x > 0) {
                    tty << "\x1B[" << m.x + 1 << 'F';
                }
                else {
                    tty << '\r';
                }
                return tty;
            }
        }

        ttystream_base&
        erase_line_from_cursor(ttystream_base& tty) {
            tty << "\x1B[K";
            return tty;
        }

        style&
        style::operator+= (style const& rhs) noexcept {
            if (!foreground && rhs.foreground) foreground = rhs.foreground;
            if (!background && rhs.background) background = rhs.background;
            if (!boldness   && rhs.boldness  ) boldness   = rhs.boldness;
            if (!font       && rhs.font      ) font       = rhs.font;
            if (!underline  && rhs.underline ) underline  = rhs.underline;
            return *this;
        }

        style
        dull_colour(detail::colour const c) {
            return style {
                std::make_optional(
                    std::make_pair(
                        detail::intensity::dull, c)),
                {}, {}, {}, {}
            };
        }

        style
        colour(detail::colour const c) {
            return style {
                std::make_optional(
                    std::make_pair(
                        detail::intensity::vivid, c)),
                {}, {}, {}, {}
            };
        }

        style
        dull_bg_colour(detail::colour const c) {
            return style {
                {},
                std::make_optional(
                    std::make_pair(
                        detail::intensity::dull, c)),
                {}, {}, {}
            };
        }

        style
        bg_colour(detail::colour const c) {
            return style {
                {},
                std::make_pair(
                    detail::intensity::vivid, c),
                {}, {}, {}
            };
        }

        style const bold = style {
            {}, {}, detail::boldness::bold, {}, {}
        };

        style const faint = style {
            {}, {}, detail::boldness::faint, {}, {}
        };

        style const italicised = style {
            {}, {}, {}, detail::font::italics, {}
        };

        style const underlined = style {
            {}, {}, {}, {}, detail::underline::single
        };
    }
}
