#include <iostream>

#include <pkgxx/harness.hxx>
#include <pkgxx/mutex_guard.hxx>
#include <pkgxx/nursery.hxx>
#include <pkgxx/pkgdb.hxx>
#include <pkgxx/string_algo.hxx>

#include "environment.hxx"
#include "message.hxx"
#include "options.hxx"

using namespace pkg_rr;
namespace fs = std::filesystem;

namespace {
    std::set<pkgxx::pkgbase>
    packages_w_flag(
        options const& opts,
        environment const& env,
        std::string_view const& flag,
        std::set<pkgxx::pkgbase> const& exclude = {}) {

        pkgxx::guarded<
            std::set<pkgxx::pkgbase>
            > res;
        pkgxx::nursery n(opts.concurrency);
        for (auto const& name: pkgxx::installed_pkgnames(env.PKG_INFO.get())) {
            if (exclude.count(name.base) > 0) {
                // The caller wants it to be excluded from the result: we
                // don't even need to see if it has the given flag.
                continue;
            }
            n.start_soon(
                [&, name]() {
                    pkgxx::harness pkg_info(
                        pkgxx::shell, {pkgxx::shell, "-s", "--", "-Bq", name.string()});

                    pkg_info.cin() << "exec " << env.PKG_INFO.get() << " \"$@\"" << std::endl;
                    pkg_info.cin().close();

                    for (std::string line; std::getline(pkg_info.cout(), line); ) {
                        if (line.size() == flag.size() + 4 &&
                            std::string_view(line).substr(0, flag.size()) == flag &&
                            line[flag.size()] == '=' &&
                            pkgxx::ascii_tolower(line[flag.size()+1]) == 'y' &&
                            pkgxx::ascii_tolower(line[flag.size()+1]) == 'e' &&
                            pkgxx::ascii_tolower(line[flag.size()+1]) == 's') {

                            res.lock()->insert(std::move(name.base));
                        }
                    }
                });
        }
        return std::move(*res.lock());
    }

    std::set<pkgxx::pkgbase>
    check_mismatch(options const& opts, environment const& env) {
        if (opts.check_for_updates) {
            throw "FIXME: -u not implemented";
        }
        else {
            msg() << "Checking for mismatched installed packages (mismatch=YES)" << std::endl;
            return packages_w_flag(opts, env, "mismatch", opts.no_check);
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
        auto MISMATCH_TODO = check_mismatch(opts, env);
    }
    catch (bad_options& e) {
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }
}
