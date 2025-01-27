#pragma once

#include <cstddef> // for std::size_t
#include <exception>
#include <optional>

#include <pkgxx/fdstream.hxx>

namespace pkgxx {
    /** Return true iff the given file descriptor refers to a terminal. */
    bool
    cisatty(int fd);

    template <typename T>
    struct dimension {
        int width;
        int height;
    };

    struct not_a_tty: virtual std::runtime_error {
#if !defined(DOXYGEN)
        not_a_tty(int const fd)
            : std::runtime_error("the file descriptor does not refer to a tty")
            , fd(fd) {}
#endif

        int const fd;
    };

    /** \ref ttystream is a subclass of \c std::iostream that additionally
     * supports operations specific to terminal devices.
     */
    struct ttystream: public fdstream {
        /** Construct a \ref ttystream out of a file descriptor \c
         * fd. Throw \ref not_a_tty If \c fd does not refer to a tty.
         */
        ttystream(int fd, bool owned = false);

        virtual ~ttystream() {}

        /** Obtain the size of the terminal. Return \c std::nullopt if
         * obtaining size is not supported on this platform.
         */
        std::optional<dimension<std::size_t>>
        size() const;
    };

    inline ttystream&
    operator<< (ttystream& tty, ttystream& (*manip)(ttystream&)) {
        return manip(tty);
    }

    /** Output manipulators for tty streams.
     */
    namespace tty {
        namespace detail {
            struct move_to {
                /// This cannot be optional because of ANSI.
                std::size_t x;
                std::optional<std::size_t> y;
            };

            ttystream&
            operator<< (ttystream& tty, move_to const& m);
        }

        /** Move the cursor to a given 0-indexed column.
         */
        inline detail::move_to
        move_x(std::size_t const col) {
            return detail::move_to {
                .x = col,
                .y = std::nullopt
            };
        }

        /** Erase the current line from the cursor to the end.
         */
        ttystream&
        erase_line_from_cursor(ttystream& tty);
    }
}
