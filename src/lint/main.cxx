#include <exception>
#include <iostream>

#include <pkgxx/pkgname.hxx>
#include <pkgxx/varcache.hxx>
#include <thread>
#include <unistd.h>

#include "environment.hxx"
#include "lintdb.hxx"
#include "options.hxx"

namespace tty = pkgxx::tty;

namespace {
    void
    list_broken_packages(lint::environment const& env, lint::lintdb const& db) {
        for (auto const& vars: db.packages()) {
            if (!vars.BROKEN.empty()) {
                auto out = env.out();
                out << tty::colour(tty::yellow)(vars.PKGNAME) << ":";

                if (vars.BROKEN.size() > 1) {
                    out << std::endl;
                    for (auto const& reason : vars.BROKEN) {
                        using namespace tty::literals;
                        out << "  "
                            << tty::dull_colour(tty::yellow)("* "_ch << reason)
                            << std::endl;
                    }
                }
                else {
                    out << ' '
                        << tty::dull_colour(tty::yellow)(vars.BROKEN[0])
                        << std::endl;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        lint::options opts(argc, argv);
        lint::environment env(opts);

        if (opts.help) {
            opts.usage();
            return 0;
        }

        pkgxx::varcache(env.PKGSRCDIR.get())
            .default_vars()
            .enable_msg(opts.verbose)
            .populate();

        bool did_something = false;
        auto db = std::async(
            std::launch::deferred,
            [&]() {
                using namespace na::literals;
                if (opts.import_db) {
                    env.msg() << "Loading database from " << *opts.import_db << std::endl;
                    return lint::lintdb{*opts.import_db};
                }
                else {
                    return lint::lintdb{
                        env.PKGSRCDIR.get(),
                        env.all_pkgpaths.get(),
                        "concurrency"_na = env.opts.concurrency};
                }
            });

        if (opts.list_broken) {
            list_broken_packages(env, db.get());
            did_something = true;
        }

        if (auto const& path = opts.export_db; path) {
            auto const& db_ = db.get();
            env.msg() << "Saving database to " << *path << std::endl;
            db_.save(
                *path,
                opts.export_as_xml
                ? lint::lintdb::format::xml
                : lint::lintdb::format::bin);
            did_something = true;
        }

        if (did_something) {
            return 0;
        }
        else {
            opts.usage();
            return 1;
        }
    }
    catch (std::exception& e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }
}
