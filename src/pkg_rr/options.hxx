#pragma once

#include <exception>
#include <filesystem>
#include <optional>
#include <pkgxx/numeral.hxx>
#include <pkgxx/pkgname.hxx>
#include <popl.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace pkg_rr {
    struct options {
        /// Throws \c popl::invalid_option on failure.
        options(int argc, char* const argv[]);

        bool help;                                              // -h
        bool check_build_version;                               // -B
        bool just_fetch;                                        // -F
        pkgxx::positive<unsigned> concurrency;                  // -j
        bool continue_on_errors;                                // -k
        bool dry_run;                                           // -n
        bool just_replace;                                      // -r
        bool strict;                                            // -s
        bool check_for_updates;                                 // -u
        unsigned verbose;                                       // -v
        std::unordered_map<std::string, std::string> make_vars; // -D
        std::optional<std::filesystem::path> log_dir;           // -L
        std::unordered_set<pkgxx::pkgbase> no_rebuild;          // -X
        std::unordered_set<pkgxx::pkgbase> no_check;            // -x

        // Does *not* exit the program.
        void usage();

    private:
        std::filesystem::path _progname;
        popl::OptionParser _op;
    };
}
