#pragma once

#include <exception>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>

#include <pkgxx/pkgname.hxx>

namespace pkg_rr {
    struct bad_options: std::runtime_error {
        bad_options()
            : std::runtime_error("") {}

        using std::runtime_error::runtime_error;
    };

    struct options {
        // Throws 'bad_options' on failure.
        options(int argc, char* const argv[]);

        bool check_build_version;                     // -B
        std::map<std::string, std::string> make_vars; // -D
        bool just_fetch;                              // -F
        bool help;                                    // -h
        bool continue_on_errors;                      // -k
        std::optional<std::filesystem::path> log_dir; // -L
        bool dry_run;                                 // -n
        bool just_replace;                            // -r
        bool strict;                                  // -s
        bool check_for_updates;                       // -u
        bool verbose;                                 // -v
        std::set<pkgxx::pkgbase> no_rebuild;          // -X
        std::set<pkgxx::pkgbase> no_check;            // -x
    };

    // Does *not* exit the program.
    void usage(std::filesystem::path const& progname);
}
