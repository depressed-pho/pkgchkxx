#pragma once

#include <filesystem>
#include <optional>
#include <pkgxx/numeral.hxx>
#include <popl.hpp>

namespace lint {
    struct options {
        /// Throws \c popl::invalid_option on failure.
        options(int argc, char* const argv[]);

        // General
        bool help;                                      // -h
        pkgxx::positive<unsigned> concurrency;          // -j
        bool verbose;                                   // -v

        // Makefile
        bool list_broken;                               // -B

        // Misc
        std::optional<std::filesystem::path> export_db; // -E
        bool export_as_xml;                             // --xml
        std::optional<std::filesystem::path> import_db; // -I

        // Modifiers
        std::optional<std::filesystem::path> PKGSRCDIR; // -P
        bool list_makefiles;                            // -L

        // Does *not* exit the program.
        void usage() const;

    private:
        std::filesystem::path _progname;
        popl::OptionParser _op;
    };
}
