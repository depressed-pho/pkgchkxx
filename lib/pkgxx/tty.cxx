#include "config.h"

#if defined(HAVE_SYS_IOCTL_H)
#  include <sys/ioctl.h>
#endif
#include <system_error>
#include <unistd.h>

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

    ttystream::ttystream(int fd, bool owned)
        : fdstream(fd, owned) {

        if (!cisatty(fd)) {
            throw not_a_tty(fd);
        }
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
        return dimension<std::size_t> {
            .width  = ws.ws_col,
            .height = ws.ws_row
        };

#else
        // Dangit, this platform doesn't have TIOCGWINSZ. It's probably
        // Illumos.
        //
        // We could open the controlling terminal device, temporarily turn
        // off ICANON and ECHO, move the cursor to the rightmost column,
        // request the cursor position to tty, read a response from tty,
        // and then parse it. But this complicates matter a lot, like damn
        // fucking a LOT. We can't bear with the complexity in this already
        // suboptimal case. No operating systems relevant today lack
        // TIOCGWINSZ after all.
        return std::nullopt;
#endif
    }

    namespace tty {
        namespace detail {
            ttystream&
            operator<< (ttystream& tty, move_to const& m) {
                if (m.y) {
                    tty << "\e[" << *m.y + 1 << ';' << m.x + 1 << 'H';
                }
                else if (m.x > 0) {
                    tty << "\e[" << m.x + 1 << 'F';
                }
                else {
                    tty << '\r';
                }
                return tty;
            }
        }

        ttystream&
        erase_line_from_cursor(ttystream& tty) {
            tty << "\e[K";
            return tty;
        }
    }
}
