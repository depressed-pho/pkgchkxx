#include <exception>
#include <iostream>

#include "environment.hxx"
#include "message.hxx"
#include "options.hxx"
#include "todo.hxx"

int main(int argc, char* argv[]) {
    using namespace pkg_chk;
    try {
        options opts(argc, argv);

        verbose(opts) << "ARGV:";
        for (int i = 0; i < argc; i++) {
            verbose(opts) << " " << argv[i];
        }
        verbose(opts) << std::endl;

        environment env(opts);
        switch (opts.mode) {
        case mode::HELP:
            usage(argv[0]);
            return 1;

        case mode::LOOKUP_TODO:
            lookup_todo(env);
            break;
        }
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
