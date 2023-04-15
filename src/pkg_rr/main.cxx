#include <iostream>

#include "environment.hxx"
#include "message.hxx"
#include "scan.hxx"
#include "options.hxx"

using namespace pkg_rr;
namespace fs = std::filesystem;

using todo_type = std::set<pkgxx::pkgbase>;

namespace {
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

    std::future<todo_type>
    check_mismatch(options const& opts, pkg_rr::package_scanner& scanner) {
        if (opts.check_for_updates) {
            throw "FIXME: -u not implemented";
        }
        else {
            msg() << "Checking for mismatched installed packages (mismatch=YES)" << std::endl;
            return scanner.add_axis("mismatch", opts.no_check);
        }
    }

    std::future<todo_type>
    check_rebuild(options const& opts, pkg_rr::package_scanner& scanner) {
        if (opts.just_fetch) {
            std::promise<todo_type> res;
            res.set_value({});
            return res.get_future();
        }
        else {
            msg() << "Checking for rebuild-requested installed packages (rebuild=YES)" << std::endl;
            return scanner.add_axis("rebuild");
        }
    }

    std::future<todo_type>
    check_unsafe(options const& opts, pkg_rr::package_scanner& scanner) {
        if (opts.just_fetch) {
            std::promise<todo_type> res;
            res.set_value({});
            return res.get_future();
        }
        else {
            auto const flag = opts.strict ? "unsafe_depends_strict" : "unsafe_depends";
            msg() << "Checking for unsafe installed packages (" << flag << "=YES)" << std::endl;
            return scanner.add_axis(flag);
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        options opts(argc, argv);

        if (opts.help) {
            usage(argv[0]);
            return 1;
        }

        environment env(opts);
        std::future<todo_type> MISMATCH_TODO_f;
        std::future<todo_type> REBUILD_TODO_f;
        std::future<todo_type> UNSAFE_TODO_f;
        {
            pkg_rr::package_scanner scanner(env.PKG_INFO.get(), opts.concurrency);
            MISMATCH_TODO_f = check_mismatch(opts, scanner);
            REBUILD_TODO_f  = check_rebuild(opts, scanner);
            UNSAFE_TODO_f   = check_unsafe(opts, scanner);
        }
        auto MISMATCH_TODO = MISMATCH_TODO_f.get();
        auto REBUILD_TODO  = REBUILD_TODO_f.get();
        auto UNSAFE_TODO   = UNSAFE_TODO_f.get();
    }
    catch (bad_options& e) {
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }
}
