#include <algorithm>
#include <cassert>
#include <cerrno>
#include <ctime>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <regex>

#include "config_file.hxx"
#include "environment.hxx"
#include "message.hxx"
#include "options.hxx"
#include "pkgdb.hxx"
#include "pkgpath.hxx"
#include "todo.hxx"

using namespace pkg_chk;
namespace fs = std::filesystem;

namespace {
    auto const RE_PYTHON_PREFIX = std::regex(
        "^py[0-9]+-",
        std::regex::optimize);

    void
    normalize_pkgname(pkgname& name) {
        name.base = std::regex_replace(name.base, RE_PYTHON_PREFIX, "py-");
    }

    bool
    is_binary_available(environment const& env, pkgname const& name) {
        return env.bin_pkg_summary.get().count(name) > 0;
    }

    void
    generate_conf_from_installed(options const& opts, environment const& env) {
        fs::path const& file = env.PKGCHK_CONF.get();
        verbose(opts) << "Write " << file << " based on installed packages" << std::endl;

        if (fs::exists(file)) {
            fs::path old = file;
            old += ".old";
            fs::rename(file, old);
        }

        std::ofstream out(file, std::ios_base::out | std::ios_base::trunc);
        if (!out) {
            throw std::system_error(errno, std::generic_category(), "Failed to open " + file.string());
        }
        out.exceptions(std::ios_base::badbit);

        std::time_t const now = std::time(nullptr);
        out << "# Generated automatically at "
            << std::put_time(std::localtime(&now), "%c %Z") << std::endl;

        std::deque<pkgpath> pkgpaths;
        for (pkgpath const& path: installed_pkgpaths(env)) {
            pkgpaths.push_back(path);
        }
        std::sort(pkgpaths.begin(), pkgpaths.end());

        config conf;
        for (pkgpath const& path: pkgpaths) {
            conf.emplace_back(config::pkg_def(path, std::vector<tagpat>()));
        }
        out << conf;
    }

    void
    lookup_todo(environment const& env) {
        /* Spawning pkg_info(1) isn't instantaneous. Start parsing the TODO
         * file right now to save some time. */
        auto f_todo = std::async(
            std::launch::async,
            [env]() {
                return todo_file(env.PKGSRCDIR.get() / "doc/TODO");
            });

        installed_pkgnames pkgnames(env);
        todo_file const todo(f_todo.get());

        for (pkgname name: pkgnames) {
            normalize_pkgname(name);

            auto const it = todo.find(name.base);
            if (it != todo.end()) {
                std::cout << name.base << ": " << it->second.name;
                if (!it->second.comment.empty()) {
                    std::cout << " " << it->second.comment;
                }
                std::cout << std::endl;
            }
        }
    }

    void
    print_pkgpaths_to_check(environment const& env) {
        config const conf(env.PKGCHK_CONF.get());
        for (auto const& path:
                 conf.apply_tags(
                     env.included_tags.get(), env.excluded_tags.get())) {
            std::cout << path << std::endl;
        }
    }

    void
    list_bin_pkgs(options const& opts, environment const& env) {
        pkgmap const& pm = env.bin_pkg_map.get();
        config const conf(env.PKGCHK_CONF.get());
        for (auto const& path:
                 conf.apply_tags(
                     env.included_tags.get(), env.excluded_tags.get())) {
            // Examine the newest binary package that corresponds to this
            // pkgpath.
            if (auto pkgs = pm.find(path); pkgs != pm.end()) {
                auto newest = pkgs->second.rbegin();
                assert(newest != pkgs->second.rend());

                if (!is_binary_available(env, newest->first)) {
                    fatal_later(opts)
                        << newest->first << " - no binary package found" << std::endl;
                }

                verbose(opts) << path << ": " << newest->first << std::endl;
                for (auto const& dep: newest->second.DEPENDS) {
                    verbose(opts) << "    depends on " << dep << std::endl;
                    // FIXME: implementation incomplete
                }
            }
            else {
                fatal_later(opts) << path << " - Unable to extract pkgname" << std::endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        options opts(argc, argv);

        verbose(opts) << "ARGV:";
        for (int i = 0; i < argc; i++) {
            verbose(opts) << " " << argv[i];
        }
        verbose(opts) << std::endl;

        environment env(opts);
        switch (opts.mode) {
        case mode::UNKNOWN:
            // This can't happen.
            std::cerr << "panic: unknown operation mode" << std::endl;
            std::abort();

        case mode::GENERATE_PKGCHK_CONF:
            generate_conf_from_installed(opts, env);
            break;

        case mode::HELP:
            usage(argv[0]);
            return 1;

        case mode::LIST_BIN_PKGS:
            list_bin_pkgs(opts, env);
            break;

        case mode::LOOKUP_TODO:
            lookup_todo(env);
            break;

        case mode::PRINT_PKGPATHS_TO_CHECK:
            print_pkgpaths_to_check(env);
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
