#include <exception>
#include <pkgxx/varcache.hxx>

#include "environment.hxx"
#include "options.hxx"
#include "replacer.hxx"

int main(int argc, char* argv[]) {
    try {
        pkg_rr::options opts(argc, argv);
        pkg_rr::environment env(opts);

        if (opts.help) {
            pkg_rr::usage(argv[0]);
            return 1;
        }

        pkgxx::varcache(env.PKGSRCDIR.get())
            .default_vars()
            .enable_msg(opts.verbose >= 2)
            .populate();

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
