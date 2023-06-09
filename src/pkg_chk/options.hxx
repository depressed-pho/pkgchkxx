#pragma once

#include <exception>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "tag.hxx"

namespace pkg_chk {
    enum class mode {
        ADD_DELETE_UPDATE,    // Any combinations of -a, -r, and -u
        GENERATE_PKGCHK_CONF, // -g
        HELP,                 // -h
        LIST_BIN_PKGS,        // -l
        LOOKUP_TODO,          // -N
    };

    struct bad_options: virtual std::runtime_error {
        bad_options()
            : std::runtime_error("") {}

        using std::runtime_error::runtime_error;
    };

    struct options {
        // Throws 'bad_options' on failure.
        options(int argc, char* const argv[]);

        pkg_chk::mode mode;
        bool add_missing;                       // -a
        bool check_build_version;               // -B
        bool use_binary_pkgs;                   // -b
        std::filesystem::path pkgchk_conf_path; // -C
        tagset add_tags;                        // -D
        bool no_clean;                          // -d
        bool fetch;                             // -f
        unsigned concurrency;                   // -j
        bool continue_on_errors;                // -k
        mutable std::ofstream logfile;          // -L
        bool dry_run;                           // -n
        std::filesystem::path bin_pkg_path;     // -P
        bool print_pkgpaths_to_check;           // -p
        bool list_ver_diffs;                    // -q
        bool delete_mismatched;                 // -r
        bool build_from_source;                 // -s
        tagset remove_tags;                     // -U
        bool update;                            // -u
        bool verbose;                           // -v
    };

    // Does *not* exit the program.
    void usage(std::filesystem::path const& progname);
}
