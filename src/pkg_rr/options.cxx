#include <iostream>
#include <string_view>
#include <unistd.h>

#include <pkgxx/string_algo.hxx>
#include "options.hxx"

extern "C" {
    extern char* optarg;
    extern int optind;
}

namespace {
    std::pair<std::string, std::string>
    parse_var_def(std::string_view const& str) {
        auto const equal = str.find('=');

        if (equal != std::string_view::npos) {
            return std::make_pair(
                std::string(str.substr(0, equal)),
                std::string(str.substr(equal + 1)));
        }
        else {
            std::cerr << "Bad variable definition: " << str << std::endl;
            throw pkg_rr::bad_options();
        }
    }
}

namespace pkg_rr {
    options::options(int argc, char* const argv[])
        : check_build_version(false)
        , just_fetch(false)
        , continue_on_errors(false)
        , dry_run(false)
        , just_replace(false)
        , strict(false)
        , check_for_updates(false)
        , verbose(false) {

        make_vars["IN_PKG_ROLLING_REPLACE"] = "1";

        int ch;
        while ((ch = getopt(argc, argv, "BD:FhkL:nrsuvX:x:")) != -1) {
            switch (ch) {
            case 'B':
                check_build_version = true;
                break;
            case 'D':
                make_vars.insert(parse_var_def(optarg));
                break;
            case 'F':
                just_fetch = true;
                break;
            case 'h':
                help = true;
                break;
            case 'k':
                continue_on_errors = true;
                break;
            case 'L':
                log_dir = optarg;
                break;
            case 'n':
                dry_run = true;
                break;
            case 'r':
                just_replace = true;
                break;
            case 's':
                strict = true;
                break;
            case 'u':
                check_for_updates = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 'X':
                for (auto const& pkg: pkgxx::words(optarg, ",")) {
                    no_rebuild.emplace(pkg);
                }
                break;
            case 'x':
                for (auto const& pkg: pkgxx::words(optarg, ",")) {
                    no_check.emplace(pkg);
                }
                break;
            case '?':
                throw bad_options();
            default:
                throw std::logic_error("Unhandled option: " + static_cast<char>(ch));
            }
        }
    }

    void usage(std::filesystem::path const& progname) {
        auto const& progbase = progname.filename().string();
        std::cout
            << "Usage: " << progname.string() << " [opts]" << std::endl
            << "    -h         Print this help" << std::endl
            << "    -B         Force exact pkg match - check \"Build version\"" << std::endl
            << "    -F         Fetch sources (including depends) only, don't build" << std::endl
            << "    -k         Keep running, even on error" << std::endl
            << "    -n         Display actions to be taken but don't actuall run them" << std::endl
            << "    -r         Just replace, don't create binary packages" << std::endl
            << "    -s         Replace even if the ABIs are still compatible (\"strict\")" << std::endl
            << "    -u         Check for mismatched packages and mark them as so" << std::endl
            << "    -v         Be verbose" << std::endl
            << "    -D VAR=VAL Passe given variables and values to make(1)" << std::endl
            << "    -L PATH    Log to path ({PATH}/{pkgdir}/{pkg})" << std::endl
            << "    -X PKG     Exclude PKG from being rebuilt" << std::endl
            << "    -x PKG     Exclude PKG from mismatch check" << std::endl
            << std::endl
            << progbase << " does `make replace' on one package at a time," << std::endl
            << "tsorting the packages being replaced according to their" << std::endl
            << "interdependencies, which avoids most duplicate rebuilds." << std::endl
            << std::endl
            << progbase << " can be used in one of two ways:" << std::endl
            << std::endl
            << "    - `make replace' is unsafe in that, if the replaced package's ABI" << std::endl
            << "      changes, its dependent packages may break.  If this happens, run" << std::endl
            << "      `" << progbase << "' (no arguments) to rebuild them against the" << std::endl
            << "      new version." << std::endl
            << std::endl
            << "    - `pkg_chk -u' will delete all your mismatched packages (where the" << std::endl
            << "      package version does not match the pkgsrc version), then reinstall" << std::endl
            << "      them one at a time, leaving you without those packages in the" << std::endl
            << "      meantime.  `" << progbase << " -u' will instead upgrade them in" << std::endl
            << "      place, allowing you to keep using your system in the meantime" << std::endl
            << "      (maybe...if you're lucky...because " << progbase << " replaces" << std::endl
            << "      the \"deepest\" dependency first, things could still break if that" << std::endl
            << "      happens to be a fundamental library whose ABI has changed)." << std::endl
            << std::endl;
    }
}
