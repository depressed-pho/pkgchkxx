#pragma once

#include <cstddef> // for std::size_t
#include <optional>

namespace pkgxx {
    /** Return true iff the given file descriptor refers to a terminal. */
    bool
    cisatty(int fd);

    template <typename T>
    struct dimension {
        int width;
        int height;
    };

    /** Obtain the size of the terminal. Return \c std::nullopt if \c fd
     * doesn't refer to a terminal, or obtaining size is not supported on
     * this platform.
     */
    std::optional<dimension<std::size_t>>
    term_size(int fd);
}
