#include <cstdlib>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "options.hxx"

extern "C" {
    extern char* optarg;
    extern int optind;
}

namespace lint {
    options::options(int argc, char* const argv[])
        : list_broken(false)
        , list_makefiles(false)
        , concurrency(std::max(1u, std::thread::hardware_concurrency()))
        , help(false)
        , verbose(false) {

        int ch;
        while ((ch = getopt(argc, argv, "BDLP:j:hv")) != -1) {
            switch (ch) {
            case 'B':
                list_broken = true;
                break;
            case 'D':
                std::cerr << argv[0] << ": option -D is not supported" << std::endl;
                throw bad_options();
            case 'L':
                list_makefiles = true;
                break;
            case 'P':
                PKGSRCDIR = optarg;
                break;
            case 'j':
                if (int const n = std::atoi(optarg); n > 0) {
                    concurrency = static_cast<unsigned>(n);
                }
                else {
                    std::cerr << argv[0] << ": option -j takes a positive integer" << std::endl;
                    throw bad_options();
                }
                break;
            case 'h':
                help = true;
                break;
            case 'v':
                verbose = true;
                break;
            default:
                std::string msg = "Unhandled option: ";
                msg += static_cast<char>(ch);
                throw std::logic_error(msg);
            }
        }
    }

    void usage(std::filesystem::path const& progname) {
        auto const& progbase = progname.filename().string();
        std::cerr
            << "Usage: " << progname.string() << " [options] [makefiles]" << std::endl
            << std::endl
            << "General options:" << std::endl
            << "    -h      Print this help (see lintpkgxx(1) for more information)" << std::endl
            << "    -j N    Set the concurrency" << std::endl
            << "    -v      Be verbose" << std::endl
            << std::endl
            << "Makefile options:" << std::endl
            << "    -B      List packages marked as 'BROKEN'" << std::endl
            << std::endl
            << "Modifiers:" << std::endl
            << "    -L      List each Makefile when scanned" << std::endl
            << "    -P DIR  Set PKGSRCDIR" << std::endl
            << std::endl;
    }
}
