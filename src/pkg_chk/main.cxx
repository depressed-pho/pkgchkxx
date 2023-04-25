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
#include <tuple>

#include <pkgxx/config.h>
#include <pkgxx/graph.hxx>
#include <pkgxx/harness.hxx>
#include <pkgxx/pkgdb.hxx>
#include <pkgxx/pkgpath.hxx>
#include <pkgxx/todo.hxx>

#include "pkg_chk/check.hxx"
#include "config_file.hxx"
#include "environment.hxx"
#include "message.hxx"
#include "options.hxx"

namespace fs = std::filesystem;

namespace {
    auto const RE_PYTHON_PREFIX = std::regex(
        "^py[0-9]+-",
        std::regex::optimize);

    void
    normalize_pkgname(pkgxx::pkgname& name) {
        name.base = std::regex_replace(name.base, RE_PYTHON_PREFIX, "py-");
    }

    bool
    run_cmd(
        pkg_chk::options const& opts,
        pkg_chk::environment const& env [[maybe_unused]],
        std::string const& cmd,
        std::vector<std::string> const& args,
        bool fail_ok,
        std::optional<std::filesystem::path> const& cwd = std::nullopt,
        std::function<void (std::map<std::string, std::string>&)> const& env_mod = [](auto&) {}) {

        if (opts.list_ver_diffs) {
            return true;
        }

        std::time_t const now = std::time(nullptr);
        msg(opts) << std::put_time(std::localtime(&now), "%R")
                  << cmd;
        for (auto const& arg: args) {
            msg(opts) << ' ' << arg;
        }
        if (cwd) {
            msg(opts) << " [CWD: " << cwd->string() << ']';
        }
        msg(opts) << std::endl;

        if (!opts.dry_run) {
            std::vector<std::string> argv = {pkgxx::shell, "-s", "--"};
            argv.insert(argv.end(), args.begin(), args.end());
            pkgxx::harness prog(
                pkgxx::shell, argv, cwd, env_mod,
                pkgxx::harness::fd_action::pipe,
                pkgxx::harness::fd_action::pipe,
                pkgxx::harness::fd_action::merge_with_stdout);
            prog.cin() << "exec " << cmd << " \"$@\"" << std::endl;
            prog.cin().close();

            for (std::string line; std::getline(prog.cout(), line); ) {
                msg(opts) << line << std::endl;
            }

            if (prog.wait_exit().status != 0) {
                auto const& show_error =
                    [&](auto&& out) {
                        out << '\'' << cmd << ' ' << pkgxx::stringify_argv(args) << "' failed" << std::endl;
                    };
                if (fail_ok) {
                    msg(opts) << "** ";
                    show_error(msg(opts));
                    return false;
                }
                else {
                    fatal(opts, show_error);
                }
            }
        }
        return true;
    }

    bool
    run_cmd_su(
        pkg_chk::options const& opts,
        pkg_chk::environment const& env,
        std::string const& cmd,
        std::vector<std::string> const& args,
        bool fail_ok,
        std::optional<std::filesystem::path> const& cwd = std::nullopt,
        std::function<void (std::map<std::string, std::string>&)> const& env_mod = [](auto&) {}) {

        if (!env.SU_CMD.get().empty()) {
            return run_cmd(opts, env, env.SU_CMD.get(), {cmd + ' ' + pkgxx::stringify_argv(args)}, fail_ok, cwd, env_mod);
        }
        else {
            return run_cmd(opts, env, cmd, args, fail_ok, cwd, env_mod);
        }
    }

    void
    delete_pkgs(
        pkg_chk::options const& opts,
        pkg_chk::environment const& env,
        std::map<pkgxx::pkgname, pkgxx::pkgpath> const& pkgs) {

        for (auto const& [name, _path]: pkgs) {
            if (pkgxx::is_pkg_installed(env.PKG_INFO.get(), name)) {
                run_cmd_su(opts, env, env.PKG_DELETE.get(), {"-r", name.string()}, true);
            }
        }
    }

    std::set<pkgxx::pkgpath>
    pkgpaths_to_check(pkg_chk::options const& opts, pkg_chk::environment const& env) {
        std::set<pkgxx::pkgpath> pkgpaths;
        if (opts.delete_mismatched || opts.update) {
            pkgpaths = env.installed_pkgpaths.get();
        }
        if (opts.add_missing) {
            env.PKGCHK_CONF.get(); // Force the evaluation of PKGCHK_CONF,
                                   // or verbose messages would interleave.
            verbose(opts) << "Append to PKGDIRLIST based on config "
                          << env.PKGCHK_CONF.get() << std::endl;
            pkg_chk::config const conf(env.PKGCHK_CONF.get());
            for (auto const& path:
                     conf.pkgpaths(
                         env.included_tags.get(), env.excluded_tags.get())) {
                pkgpaths.insert(path);
            }
        }
        return pkgpaths;
    }

    struct checker: virtual pkg_chk::configurable_checker_base {
        checker(pkg_chk::options const& opts, pkg_chk::environment const& env)
            : checker_base(
                opts.add_missing,
                opts.check_build_version,
                opts.concurrency,
                opts.update,
                opts.delete_mismatched,
                env.PKG_INFO)
            , source_checker_base(env.PKGSRCDIR)
            , binary_checker_base(
                env.PACKAGES,
                env.PKG_SUFX,
                env.bin_pkg_summary)
            , configurable_checker_base(opts.build_from_source)
            , _opts(opts) {}

    protected:
        virtual void
        atomic_msg(std::function<void (std::ostream&)> const& f) const override {
            pkg_chk::atomic_msg(_opts, f);
        }

        virtual void
        atomic_warn(std::function<void (std::ostream&)> const& f) const override {
            pkg_chk::atomic_warn(_opts, f);
        }

        virtual void
        atomic_verbose(std::function<void (std::ostream&)> const& f) const override {
            pkg_chk::atomic_verbose(_opts, f);
        }

        virtual void
        fatal(std::function<void (std::ostream&)> const& f) const override {
            pkg_chk::fatal(_opts, f);
        }

        pkg_chk::options const& _opts;
    };

    void
    delete_and_recheck(
        pkg_chk::options const& opts,
        pkg_chk::environment const& env,
        std::set<pkgxx::pkgpath> const& pkgpaths,
        checker const& chk,
        checker::result& res) {

        std::set<pkgxx::pkgpath> update_conf;
        if (opts.update) {
            // Save current installed set to PKGCHK_UPDATE_CONF so that
            // restarting failed update would not cause installed packages
            // to end up missing.
            fs::path const& update_conf_file = env.PKGCHK_UPDATE_CONF.get();
            if (fs::exists(update_conf_file)) {
                msg(opts) << "Merging in previous " << update_conf_file << std::endl;
                update_conf = pkg_chk::config(update_conf_file).pkgpaths();
            }

            for (pkgxx::pkgpath const& path: env.installed_pkgpaths.get()) {
                update_conf.insert(path);
            }

            if (!opts.dry_run && !opts.list_ver_diffs) {
                std::ofstream out(update_conf_file, std::ios_base::out | std::ios_base::trunc);
                if (!out) {
                    throw std::system_error(errno, std::generic_category(), "Failed to open " + update_conf_file.string());
                }
                out.exceptions(std::ios_base::badbit);
                for (pkgxx::pkgpath const& path: update_conf) {
                    out << path << std::endl;
                }
            }
        }
        if (opts.delete_mismatched || opts.update) {
            if (!res.MISMATCH_TODO.empty()) {
                delete_pkgs(opts, env, res.MISMATCH_TODO);
                msg(opts) << "Rechecking packages after deletions" << std::endl;
            }
            std::set<pkgxx::pkgpath> recheck_paths = pkgpaths;
            if (opts.update) {
                recheck_paths.insert(update_conf.begin(), update_conf.end());
            }
            if (opts.add_missing || opts.update) {
                res = chk.run(recheck_paths);
            }
        }
    }

    bool
    try_fetch(
        pkg_chk::options const& opts,
        pkg_chk::environment const& env,
        pkgxx::pkgpath const& path) {

        std::stringstream ss;
        ss << CFG_BMAKE << " -C " << (env.PKGSRCDIR.get() / path) << " fetch-list | " << pkgxx::shell;
        return run_cmd(opts, env, ss.str(), {}, true);
    }

    bool
    try_install(
        pkg_chk::options const& opts,
        pkg_chk::environment const& env,
        pkgxx::pkgname const& name,
        pkgxx::pkgpath const& path) {

        if (pkgxx::is_pkg_installed(env.PKG_INFO.get(), name)) {
            msg(opts) << name << " was installed in a previous stage" << std::endl;
            return run_cmd_su(
                opts, env, env.PKG_ADMIN.get(), {"unset", "automatic", name.string()}, true);
        }
        else if (opts.use_binary_pkgs && env.is_binary_available(name)) {
            return run_cmd_su(
                opts, env, env.PKG_ADD.get(),
                {(env.PACKAGES.get() / (name.string() + env.PKG_SUFX.get())).string()},
                true,
                std::nullopt,
                [&](auto& env_map) {
                    if (std::string const& PKG_PATH = env.PKG_PATH.get(); !PKG_PATH.empty()) {
                        env_map["PKG_PATH"] = PKG_PATH;
                    }
                });
        }
        else if (opts.build_from_source) {
            return run_cmd(
                opts, env, CFG_BMAKE,
                {"update", opts.no_clean ? "NOCLEAN=yes" : "CLEANDEPENDS=yes"},
                true,
                env.PKGSRCDIR.get() / path);
        }
        else {
            return false;
        }
    }

    void
    add_delete_update(pkg_chk::options const& opts, pkg_chk::environment const& env) {
        std::set<pkgxx::pkgpath> const pkgpaths = pkgpaths_to_check(opts, env);
        if (opts.print_pkgpaths_to_check) {
            for (pkgxx::pkgpath const& path: pkgpaths) {
                std::cout << path << std::endl;
            }
            return;
        }

        checker const chk(opts, env);
        checker::result res = chk.run(pkgpaths);
        if (!res.MISMATCH_TODO.empty() ||
            (opts.update && fs::exists(env.PKGCHK_UPDATE_CONF.get()))) {

            delete_and_recheck(opts, env, pkgpaths, chk, res);
        }

        std::set<pkgxx::pkgname> FAILED_DONE;
        if (opts.fetch && !res.MISSING_TODO.empty()) {
            // The script generated by "make fetch-list" recurse into
            // dependencies, which means we can't run it parallelly without
            // the risk of race condition.
            msg(opts) << "Fetching distfiles" << std::endl;
            for (auto const& [name, path]: res.MISSING_TODO) {
                // Packages previously marked as MISMATCH_TODO have been
                // moved to MISSING_TODO at this point.
                if (!try_fetch(opts, env, path)) {
                    FAILED_DONE.insert(name);
                }
            }
        }

        std::set<pkgxx::pkgname> INSTALL_DONE;
        if ((opts.add_missing || opts.update) && !res.MISSING_TODO.empty()) {
            msg(opts) << "Installing packages" << std::endl;
            for (auto const& [name, path]: res.MISSING_TODO) {
                if (try_install(opts, env, name, path)) {
                    INSTALL_DONE.insert(name);
                }
                else {
                    FAILED_DONE.insert(name);
                }
            }
        }

        // Delete PKGCHK_UPDATE_CONF if the update completed without any
        // errors.
        if (opts.update && FAILED_DONE.empty() && fs::exists(env.PKGCHK_UPDATE_CONF.get())) {
            fs::remove(env.PKGCHK_UPDATE_CONF.get());
        }

        if (!res.MISSING_DONE.empty()) {
            msg(opts) << "Missing:";
            for (pkgxx::pkgpath const& path: res.MISSING_DONE) {
                msg(opts) << ' ' << path;
            }
            msg(opts) << std::endl;
        }
        if (!INSTALL_DONE.empty()) {
            msg(opts) << "Installed:";
            for (pkgxx::pkgname const& name: INSTALL_DONE) {
                msg(opts) << ' ' << name;
            }
            msg(opts) << std::endl;
        }
        if (!FAILED_DONE.empty()) {
            fatal(opts,
                  [&](auto& out) {
                      out << "Failed:";
                      for (pkgxx::pkgname const& name: FAILED_DONE) {
                          msg(opts) << ' ' << name;
                      }
                      msg(opts) << std::endl;
                  });
        }
    }

    void
    generate_conf_from_installed(pkg_chk::options const& opts, pkg_chk::environment const& env) {
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

        pkg_chk::config conf;
        for (pkgxx::pkgpath const& path: env.installed_pkgpaths.get()) {
            conf.emplace_back(pkg_chk::config::pkg_def(path, std::vector<pkg_chk::tagpat>()));
        }
        out << conf;
    }

    void
    lookup_todo(pkg_chk::environment const& env) {
        /* Spawning pkg_info(1) isn't instantaneous. Start parsing the TODO
         * file right now to save some time. */
        auto f_todo = std::async(
            std::launch::async,
            [env]() {
                return pkgxx::todo_file(env.PKGSRCDIR.get() / "doc/TODO");
            });

        std::set<pkgxx::pkgname> const& pkgnames = env.installed_pkgnames.get();
        pkgxx::todo_file const todo(f_todo.get());
        for (pkgxx::pkgname name: pkgnames) {
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
    list_bin_pkgs(pkg_chk::options const& opts, pkg_chk::environment const& env) {
        std::string    const& sufx = env.PKG_SUFX.get();
        pkgxx::summary const& sum  = env.bin_pkg_summary.get();
        pkgxx::pkgmap  const& pm   = env.bin_pkg_map.get();
        pkg_chk::config const conf(env.PKGCHK_CONF.get());

        // TODO: We don't take account of SUPERSEDES but how do we do it?

        using pkgname_cref = std::reference_wrapper<pkgxx::pkgname const>;
        std::map<pkgname_cref, pkgxx::pkgvars const&> to_list;
        pkgxx::graph<pkgname_cref> topology;

        for (auto const& path:
                 conf.pkgpaths(
                     env.included_tags.get(), env.excluded_tags.get())) {
            if (auto pkgbases = pm.find(path); pkgbases != pm.end()) {
                // For each PKGBASE that correspond to this PKGPATH, find
                // the latest binary package and schedule it for listing.
                for (auto const& [_base, sum]: pkgbases->second) {
                    auto latest = sum.rbegin();
                    assert(latest != sum.rend());

                    if (env.is_binary_available(latest->first)) {
                        to_list.insert(*latest);
                    }
                    else {
                        fatal_later(opts)
                            << latest->first << " - no binary package found" << std::endl;
                    }
                }
            }
            else {
                fatal_later(opts) << path << " - Unable to extract pkgname" << std::endl;
            }
        }

        while (!to_list.empty()) {
            for (auto const& [name, _vars]: to_list) {
                topology.add_vertex(name);
            }

            decltype(to_list) scheduled;
            for (auto const& [name, vars]: to_list) {
                verbose(opts) << vars.PKGPATH << ": " << name << std::endl;
                for (auto const& dep_pattern: vars.DEPENDS) {
                    verbose(opts) << "    depends on " << dep_pattern << ": ";
                    if (auto const best = dep_pattern.best(sum); best != sum.end()) {
                        pkgxx::pkgname const& dep = best->first;

                        verbose(opts) << dep << std::endl;
                        if (!topology.has_vertex(dep)) {
                            scheduled.insert(*best);
                        }
                        topology.add_edge(name, dep);
                    }
                    else {
                        verbose(opts) << "(nothing matches)" << std::endl;
                        fatal_later(opts) << name << ": missing dependency " << dep_pattern << std::endl;
                    }
                }
            }
            to_list = std::move(scheduled);
        }

        try {
            for (auto name: topology.tsort()) {
                std::cout << name << sufx << std::endl;
            }
        }
        catch (pkgxx::not_a_dag<pkgname_cref>& e) {
            // This exception contains reference wrappers to pkgname, which
            // means we can't just let it go down the stack because those
            // wrappers would become dangling at some point.
            fatal(opts, [&](auto& out) {
                            out << e.what() << std::endl;
                        });
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        pkg_chk::options opts(argc, argv);

        verbose(opts) << "ARGV:";
        for (int i = 0; i < argc; i++) {
            verbose(opts) << " " << argv[i];
        }
        verbose(opts) << std::endl;

        pkg_chk::environment env(opts);
        switch (opts.mode) {
        case pkg_chk::mode::ADD_DELETE_UPDATE:
            add_delete_update(opts, env);
            break;

        case pkg_chk::mode::GENERATE_PKGCHK_CONF:
            generate_conf_from_installed(opts, env);
            break;

        case pkg_chk::mode::HELP:
            pkg_chk::usage(argv[0]);
            return 1;

        case pkg_chk::mode::LIST_BIN_PKGS:
            list_bin_pkgs(opts, env);
            break;

        case pkg_chk::mode::LOOKUP_TODO:
            lookup_todo(env);
            break;

        default:
            std::cerr << "panic: unknown operation mode" << std::endl;
            std::abort();
        }
        return 0;
    }
    catch (pkg_chk::bad_options& e) {
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }
}
