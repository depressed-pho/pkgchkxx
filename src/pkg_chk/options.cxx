#include <cstdlib>
#include <iostream>
#include <optional>
#include <thread>
#include <unistd.h>
#include <vector>

#include "options.hxx"

using namespace std::literals;

extern "C" {
    extern char* optarg;
    extern int optind;
}

namespace pkg_chk {
    options::options(int argc, char* const argv[])
        : add_missing(false)
        , check_build_version(false)
        , use_binary_pkgs(false)
        , no_clean(false)
        , fetch(false)
        , concurrency(std::max(1u, std::thread::hardware_concurrency()))
        , continue_on_errors(false)
        , dry_run(false)
        , print_pkgpaths_to_check(false)
        , list_ver_diffs(false)
        , delete_mismatched(false)
        , build_from_source(false)
        , update(false)
        , verbose(false) {

        std::optional<pkg_chk::mode> mode_;
        int ch;
        while ((ch = getopt(argc, argv, "BC:D:L:P:U:abcdfghij:klNnpqrsuv")) != -1) {
            switch (ch) {
            case 'a':
                mode_       = mode::ADD_DELETE_UPDATE;
                add_missing = true;
                break;
            case 'B':
                check_build_version = true;
                break;
            case 'b':
                use_binary_pkgs = true;
                break;
            case 'C':
                pkgchk_conf_path = optarg;
                break;
            case 'c':
                std::cerr << argv[0] << ": option -c is deprecated. Use -a -q" << std::endl;
                mode_          = mode::ADD_DELETE_UPDATE;
                add_missing    = true;
                list_ver_diffs = true;
                break;
            case 'D':
                add_tags = tagset(std::string_view(optarg));
                break;
            case 'f':
                fetch = true;
                break;
            case 'g':
                mode_ = mode::GENERATE_PKGCHK_CONF;
                break;
            case 'h':
                mode_ = mode::HELP;
                break;
            case 'i':
                std::cerr << argv[0] << ": option -i is deprecated. Use -u -q" << std::endl;
                mode_          = mode::ADD_DELETE_UPDATE;
                update         = true;
                list_ver_diffs = true;
                break;
            case 'j':
                if (int const n = std::atoi(optarg); n > 0) {
                    concurrency = n;
                }
                else {
                    std::cerr << argv[0] << ": option -j takes a positive integer" << std::endl;
                    throw bad_options();
                }
                break;
            case 'k':
                continue_on_errors = true;
                break;
            case 'L':
                logfile.open(optarg, std::ios_base::app);
                if (!logfile) {
                    throw std::system_error(errno, std::generic_category(), "Failed to open "s + optarg);
                }
                logfile.exceptions(std::ios_base::badbit);
                break;
            case 'l':
                mode_ = mode::LIST_BIN_PKGS;
                break;
            case 'N':
                mode_ = mode::LOOKUP_TODO;
                break;
            case 'n':
                dry_run = true;
                break;
            case 'p':
                print_pkgpaths_to_check = true;
                break;
            case 'P':
                bin_pkg_path = optarg;
                break;
            case 'q':
                list_ver_diffs = true;
                break;
            case 'r':
                mode_             = mode::ADD_DELETE_UPDATE;
                delete_mismatched = true;
                break;
            case 's':
                build_from_source = true;
                break;
            case 'U':
                remove_tags = tagset(std::string_view(optarg));
                break;
            case 'u':
                mode_  = mode::ADD_DELETE_UPDATE;
                update = true;
                break;
            case 'v':
                verbose = true;
                break;
            case '?':
                throw bad_options();
            default:
		std::string msg = "Unhandled option: ";
		msg += static_cast<char>(ch);
                throw std::logic_error(msg);
            }
        }

        if (!use_binary_pkgs && !build_from_source) {
            use_binary_pkgs   = true;
            build_from_source = true;
        }

        if (mode_) {
            mode = *mode_;
        }
        else {
            std::cerr
                << argv[0]
                << ": must specify at least one of -a, -g, -l, -r, -u, or -N" << std::endl;
            throw bad_options();
        }

        if (fetch && !build_from_source) {
            std::cerr
                << argv[0]
                << ": -f is an option to pre-fetch source distributions to build packages,"
                << " which does not make sense if one doesn't intend to build them" << std::endl;
            throw bad_options();
        }

        if (argc > optind) {
            std::cerr
                << argv[0]
                << ": an additional argument is given: " << argv[optind] << std::endl;
            throw bad_options();
        }
    }

    void usage(std::filesystem::path const& progname) {
        std::cout
            << "Usage: " << progname.string() << " [opts]" << std::endl
            << "    -a       Add all missing packages" << std::endl
            << "    -B       Force exact pkg match - check \"Build version\" & even downgrade" << std::endl
            << "    -b       Use binary packages" << std::endl
            << "    -C conf  Use pkgchk.conf file 'conf'" << std::endl
            << "    -D tags  Comma separated list of additional pkgchk.conf tags to set" << std::endl
            << "    -d       Do not clean the pkg build directories" << std::endl
            << "    -f       Perform a 'make fetch' for all required packages" << std::endl
            << "    -g       Generate an initial pkgchk.conf file" << std::endl
            << "    -h       Print this help" << std::endl
            << "    -j conc  Parallelize certain operations with a given concurrency" << std::endl
            << "    -k       Continue with further packages if errors are encountered" << std::endl
            << "    -L file  Redirect output from commands run into file (should be fullpath)" << std::endl
            << "    -l       List binary packages including dependencies" << std::endl
            << "    -N       List installed packages for which a newer version is in TODO" << std::endl
            << "    -n       Display actions that would be taken, but do not perform them" << std::endl
            << "    -p       Display the list of pkgpaths that match the current tags" << std::endl
            << "    -P dir   Set PACKAGES dir (overrides any other setting)" << std::endl
            << "    -q       Do not display actions or take any action; only list packages" << std::endl
            << "    -r       Recursively remove mismatches (use with care)" << std::endl
            << "    -s       Use source for building packages" << std::endl
            << "    -U tags  Comma separated list of pkgchk.conf tags to unset ('*' for all)" << std::endl
            << "    -u       Update all mismatched packages" << std::endl
            << "    -v       Be verbose" << std::endl
            << std::endl
            << "pkg_chk verifies installed packages against pkgsrc." << std::endl
            << "The most common usage is 'pkg_chk -u -q' to check all installed packages or" << std::endl
            << "'pkg_chk -u' to update all out of date packages." << std::endl
            << "For more advanced usage, including defining a set of desired packages based" << std::endl
            << "on hostname and type, see pkg_chk(8)." << std::endl
            << std::endl
            << "If neither -b nor -s is given, both are assumed with -b preferred." << std::endl
            << std::endl;
    }
}
