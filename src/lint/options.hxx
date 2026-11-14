#pragma once

#include <exception>
#include <filesystem>
#include <optional>

namespace lint {
    struct bad_options: public std::runtime_error {
        bad_options()
            : std::runtime_error("") {}

        using std::runtime_error::runtime_error;
    };

    struct options {
        /// Throws 'bad_options' on failure.
        options(int argc, char* const argv[]);

        bool list_broken;                               // -B
        bool list_makefiles;                            // -L
        std::optional<std::filesystem::path> PKGSRCDIR; // -P
        unsigned concurrency;                           // -j
        bool help;                                      // -h
        bool verbose;                                   // -v
    };

    // Does *not* exit the program.
    void usage(std::filesystem::path const& progname);
}
