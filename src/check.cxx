#include <cassert>
#include <thread>

#include <pkgxx/build_version.hxx>
#include <pkgxx/makevars.hxx>
#include <pkgxx/mutex_guard.hxx>
#include <pkgxx/nursery.hxx>

#include "check.hxx"
#include "config_file.hxx"
#include "message.hxx"

namespace fs = std::filesystem;

namespace {
    using namespace pkg_chk;

    std::set<pkgxx::pkgname>
    latest_pkgnames_from_source(
        options const& opts,
        environment const& env,
        pkgxx::pkgpath const& path) {
        // There are simply no means to enumerate every possible PKGNAME a
        // PKGPATH can provide. So we first extract the default PKGNAME
        // from it, then retrieve other PKGNAMEs according to installed
        // packages. This means:
        //
        // * pkg_chk -a: We'll mark the default PKGNAME as either
        //   MISSING_TODO or OK.
        //
        // * pkg_chk -u: We'll mark installed packages as either
        //   MISMATCH_TODO or OK, and may also mark the default PKGNAME as
        //   MISSING_TODO or OK. MISSING_TODO will be ignored unless -a is
        //   also given so this shouldn't be a problem.
        //
        // * pkg_chk -r: Same as above.
        //
        if (!fs::exists(env.PKGSRCDIR.get() / path / "Makefile")) {
            atomic_warn(
                opts,
                [&](auto& out) {
                    out << "No " << path << "/Makefile - package moved or obsolete?" << std::endl;
                });
            return {};
        }

        auto const default_pkgname =
            pkgxx::extract_pkgmk_var<pkgxx::pkgname>(env.PKGSRCDIR.get() / path, "PKGNAME");
        if (!default_pkgname) {
            fatal(
                opts,
                [&](auto& out) {
                    out << "Unable to extract PKGNAME for " << path << std::endl;
                });
        }

        // We need to search non-default PKGNAMEs only when -u or -r is
        // given, otherwise -a would install every single PKGNAME that the
        // PKGPATH provides.
        std::set<pkgxx::pkgname> pkgnames = {
            pkgxx::pkgname(*default_pkgname)
        };
        if (opts.update || opts.delete_mismatched) {
            auto const& pm = env.installed_pkgpaths_with_pkgnames.get();
            if (auto installed_pkgnames = pm.find(path); installed_pkgnames != pm.end()) {
                for (auto const& installed_pkgname: installed_pkgnames->second) {
                    if (installed_pkgname.base != default_pkgname->base) {
                        // We found a non-default PKGBASE but spawning
                        // make(1) takes seriously long. It's really
                        // tempting to cheat by making up a PKGNAME by
                        // combining it with the already known PKGVERSION,
                        // but we can't. This is because previously
                        // supported Python versions (or Ruby, or Lua, or
                        // whatever) may have become unsupported by this
                        // PKGPATH, and we must treat it like a removed
                        // package in that case.
                        auto const alternative_pkgname =
                            pkgxx::extract_pkgmk_var<pkgxx::pkgname>(
                                env.PKGSRCDIR.get() / path,
                                "PKGNAME",
                                {{"PKGNAME_REQD", installed_pkgname.base + "-[0-9]*"}}).value();
                        // If it doesn't support this PKGNAME_REQD, it
                        // reports a PKGNAME whose PKGBASE doesn't match
                        // the requested one.
                        if (alternative_pkgname.base == installed_pkgname.base) {
                            pkgnames.insert(std::move(alternative_pkgname));
                        }
                        else {
                            atomic_warn(
                                opts,
                                [&](auto& out) {
                                    out << path << " had presumably provided a package named like "
                                        << installed_pkgname.base << "-[0-9]* but it no longer does so. "
                                        << "The installed package " << installed_pkgname
                                        << " cannot be updated. Delete it and re-run the command."
                                        << std::endl;
                                });
                            return {};
                        }
                    }
                }
            }
        }

        return pkgnames;
    }

    std::set<pkgxx::pkgname>
    latest_pkgnames_from_binary(
        options const& opts,
        environment const& env,
        pkgxx::pkgpath const& path) {
        // We can enumerate every possible PKGNAME a PKGPATH can provide
        // just by querying the binary package summary. However, we cannot
        // know which one is the default without consulting the
        // source. This is problematic when -a is given. The best
        // workaround we can do is to sort the pkgbases and pick the last
        // one, but this is of course not guaranteed to pick the correct
        // package.
        auto const& pm = env.bin_pkg_map.get();
        if (auto pkgbases = pm.find(path); pkgbases != pm.end()) {
            std::set<pkgxx::pkgname> pkgnames;
            if (opts.add_missing) {
                // Guess the default pkgbase. This may be inaccurate.
                auto guessed_default = pkgbases->second.rbegin();
                assert(guessed_default != pkgbases->second.rend());

                auto latest = guessed_default->second.rbegin();
                assert(latest != guessed_default->second.rend());

                pkgnames.insert(latest->first);
            }
            if (opts.update || opts.delete_mismatched) {
                // We need to enumerate only PKGBASEs that are already
                // installed, otherwise -a would install every single
                // PKGNAME that the PKGPATH provides.
                auto const& installed_pkgbases = env.installed_pkgbases.get();
                for (auto const& base: pkgbases->second) {
                    if (installed_pkgbases.find(base.first) != installed_pkgbases.end()) {
                        auto latest = base.second.rbegin();
                        assert(latest != base.second.rend());

                        pkgnames.insert(latest->first);
                    }
                }
            }
            return pkgnames;
        }
        else {
            // No binary packages provided by the given pkgpath are found.
            return {};
        }
    }
}

namespace pkg_chk {
    // This is the slowest part of pkg_chk. For each package we need to
    // extract variables from package Makefiles unless we are using binary
    // packages. Luckily for us each check is independent of each other so
    // we can parallelise them.
    check_result
    check_installed_packages(
        options const& opts,
        environment const& env,
        std::set<pkgxx::pkgpath> const& pkgpaths) {

        pkgxx::guarded<check_result> res;
        pkgxx::nursery n;
        for (pkgxx::pkgpath const& path: pkgpaths) {
            n.start_soon(
                [&]() {
                    // Find the set of latest PKGNAMEs provided by this
                    // PKGPATH. Most PKGPATHs have just one corresponding
                    // PKGNAME but some (py-*) have more.
                    std::set<pkgxx::pkgname> const latest_pkgnames
                        = opts.build_from_source
                        ? latest_pkgnames_from_source(opts, env, path)
                        : latest_pkgnames_from_binary(opts, env, path);

                    if (latest_pkgnames.empty()) {
                        res.lock()->MISSING_DONE.insert(path);
                        return;
                    }

                    auto const& installed_pkgnames = env.installed_pkgnames.get();
                    for (pkgxx::pkgname const& name: latest_pkgnames) {
                        if (auto installed = installed_pkgnames.lower_bound(
                                pkgxx::pkgname(name.base, pkgxx::pkgversion()));
                            installed != installed_pkgnames.end() && installed->base == name.base) {

                            if (installed->version == name.version) {
                                // The latest PKGNAME turned out to be
                                // installed. Good, but that's not enough
                                // if -B is given.
                                if (opts.check_build_version) {
                                    std::optional<pkgxx::build_version> latest_build_version;
                                    if (opts.use_binary_pkgs && !opts.build_from_source) {
                                        if (auto const file = env.binary_package_file_of(name); file) {
                                            latest_build_version =
                                                pkgxx::build_version::from_binary(env.PKG_INFO.get(), *file);
                                        }
                                    }
                                    else {
                                        latest_build_version =
                                            pkgxx::build_version::from_source(env.PKGSRCDIR.get(), path);
                                    }
                                    auto const installed_build_version =
                                        pkgxx::build_version::from_installed(env.PKG_INFO.get(), *installed);
                                    assert(installed_build_version.has_value());

                                    if (latest_build_version.has_value()) {
                                        if (latest_build_version == installed_build_version) {
                                            atomic_verbose(
                                                opts,
                                                [&](auto& out) {
                                                    out << path << " - " << name << " OK" << std::endl;
                                                });
                                        }
                                        else {
                                            atomic_msg(
                                                opts,
                                                [&](auto& out) {
                                                    out << path << " - " << name << " build_version mismatch" << std::endl;
                                                });
                                            atomic_verbose(
                                                opts,
                                                [&](auto& out) {
                                                    out << "--current--"                   << std::endl
                                                        << latest_build_version.value()
                                                        << "--installed--"                 << std::endl
                                                        << installed_build_version.value()
                                                        << "----"                          << std::endl
                                                        << std::endl;
                                                });
                                            res.lock()->MISMATCH_TODO.insert(*installed);
                                        }
                                    }
                                    else {
                                        atomic_msg(
                                            opts,
                                            [&](auto& out) {
                                                out << path << " - " << name << " build_version missing" << std::endl;
                                            });
                                    }
                                }
                                else {
                                    atomic_verbose(
                                        opts,
                                        [&](auto& out) {
                                            out << path << " - " << name << " OK" << std::endl;
                                        });
                                }
                            }
                            else if (installed->version < name.version) {
                                // We have an older version installed.
                                atomic_msg(
                                    opts,
                                    [&](auto& out) {
                                        out << path << " - " << *installed << " < " << name
                                            << (env.is_binary_available(name) ? " (has binary package)" : "")
                                            << std::endl;
                                    });
                                res.lock()->MISMATCH_TODO.insert(*installed);
                            }
                            else {
                                // We have a newer version installed
                                // but how can that happen?
                                if (opts.check_build_version) {
                                    atomic_msg(
                                        opts,
                                        [&](auto& out) {
                                            out << path << " - " << *installed << " > " << name
                                                << (env.is_binary_available(name) ? " (has binary package)" : "")
                                                << std::endl;
                                        });
                                    res.lock()->MISMATCH_TODO.insert(*installed);
                                }
                                else {
                                    atomic_msg(
                                        opts,
                                        [&](auto& out) {
                                            out << path << " - " << *installed << " > " << name << " - ignoring"
                                                << (env.is_binary_available(name) ? " (has binary package)" : "")
                                                << std::endl;
                                        });
                                }
                            }
                        }
                        else {
                            atomic_msg(
                                opts,
                                [&](auto& out) {
                                    out << path << " - " << name << " missing"
                                        << (env.is_binary_available(name) ? " (has binary package)" : "")
                                        << std::endl;
                                });
                            res.lock()->MISSING_TODO.insert_or_assign(name, path);
                        }
                    }
                });
        }
        return std::move(*res.lock());
    }
}
