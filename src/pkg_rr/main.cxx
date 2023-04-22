#include <exception>

#include "environment.hxx"
#include "options.hxx"
#include "replacer.hxx"

int main(int argc, char* argv[]) {
    try {
        pkg_rr::options opts(argc, argv);

        if (opts.help) {
            pkg_rr::usage(argv[0]);
            return 1;
        }

        pkg_rr::environment env(opts);
        pkg_rr::rolling_replacer(argv[0], opts, env).run();
    }
    catch (pkg_rr::bad_options& e) {
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }
}
