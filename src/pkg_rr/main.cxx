#include <exception>

#include "environment.hxx"
#include "options.hxx"
#include "replacer.hxx"

/* FIXME

    todo_type
    packages_w_flag(
        options const& opts,
        environment const& env,
        std::string const& flag,
        std::set<pkgxx::pkgbase> const& exclude = {}) {

        std::future<todo_type> res;
        {
            pkg_rr::package_scanner scanner(env.PKG_INFO.get(), opts.concurrency);
            res = scanner.add_axis(flag, exclude);
        }
        return res.get();
    }
*/

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
