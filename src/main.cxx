#include <exception>
#include <iostream>

#include "environment.hxx"
#include "message.hxx"
#include "options.hxx"

int main(int argc, char* argv[]) {
    using namespace pkg_chk;

    try {
        options opts(argc, argv);

        verbose(opts) << "ARGV:";
        for (int i = 0; i < argc; i++) {
            verbose(opts) << " " << argv[i];
        }
        verbose(opts) << "\n";

        if (opts.mode == mode::HELP) {
            usage(argv[0]);
            return 1;
        }

        environment env(opts);

        return 0;
    }
    catch (bad_options& e) {
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }
}
