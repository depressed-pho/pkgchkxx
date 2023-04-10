#include <iostream>

#include "options.hxx"

using namespace pkg_rr;
namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    try {
        options opts(argc, argv);

        if (opts.help) {
            usage(argv[0]);
            return 1;
        }

        
    }
    catch (bad_options& e) {
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }
}
