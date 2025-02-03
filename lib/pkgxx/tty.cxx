#include "config.h"

#if defined(HAVE_SYS_IOCTL_H)
#  include <sys/ioctl.h>
#endif
#include <system_error>
#include <unistd.h>

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

    namespace tty {
        namespace detail {
            ttystream&
            operator<< (ttystream& tty, move_to const& m) {
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

        ttystream&
        erase_line_from_cursor(ttystream& tty) {
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

        ttystream&
        operator<< (ttystream& tty, style const& sty) {
            if (!tty.use_colour()) {
                return tty;
            }

            tty << "\x1B[0";
            if (sty.foreground) {
                auto const& [i, c] = *sty.foreground;
                tty << ';'
                    << 30 + static_cast<int>(i) + static_cast<int>(c);
            }
            if (sty.background) {
                auto const& [i, c] = *sty.background;
                tty << ';'
                    << 40 + static_cast<int>(i) + static_cast<int>(c);
            }
            if (sty.boldness) {
                tty << ';'
                    << static_cast<int>(*sty.boldness);
            }
            if (sty.font) {
                tty << ';'
                    << static_cast<int>(*sty.font);
            }
            if (sty.underline) {
                tty << ';'
                    << static_cast<int>(*sty.underline);
            }
            tty << 'm';
            return tty;
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
