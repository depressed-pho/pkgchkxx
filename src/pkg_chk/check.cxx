#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <thread>

#include <pkgxx/makevars.hxx>
#include <pkgxx/mutex_guard.hxx>
#include <pkgxx/nursery.hxx>

#include "check.hxx"

namespace fs = std::filesystem;

namespace pkg_chk {
    checker_base::checker_base(
        bool add_missing,
        bool check_build_version,
        unsigned concurrency,
        bool update,
        bool delete_mismatched,
        std::shared_future<std::string> const& PKG_INFO)
        : _add_missing(add_missing)
        , _check_build_version(check_build_version)
        , _concurrency(concurrency)
        , _update(update)
        , _delete_mismatched(delete_mismatched)
        , _PKG_INFO(PKG_INFO)
        , _installed_pkg_summary(
            std::async(
                std::launch::deferred,
                [this]() {
                    atomic_verbose(
                        [](auto& out) {
                            out << "Getting summary from installed packages" << std::endl;
                        });
                    return pkgxx::summary(_PKG_INFO.get());
                }).share())
        , _installed_pkgnames(
            std::async(
                std::launch::deferred,
                [this]() {
                    std::set<pkgxx::pkgname> ret;
                    for (auto const& [name, _vars]: _installed_pkg_summary.get()) {
                        ret.insert(name);
                    }
                    return ret;
                }).share()) {}

    checker_base::result
    checker_base::run(std::set<pkgxx::pkgpath> const& pkgpaths) const {
        total(pkgpaths.size());

        // This is the slowest part of pkg_chk. For each package we need to
        // extract variables from package Makefiles unless we are using
        // binary packages. Luckily for us each check is independent of
        // each other so we can parallelise them.
        pkgxx::guarded<result> res;
        {
            pkgxx::nursery n(_concurrency);
            for (pkgxx::pkgpath const& path: pkgpaths) {
                n.start_soon(
                    [&]() {
                        // Find the set of latest PKGNAMEs provided by this
                        // PKGPATH. Most PKGPATHs have just one
                        // corresponding PKGNAME but some (py-*) have more.
                        auto const latest_pkgnames = find_latest_pkgnames(path);
                        if (latest_pkgnames.empty()) {
                            res.lock()->MISSING_DONE.insert(path);
                            progress();
                            return;
                        }

                        auto const& installed_pkgnames = _installed_pkgnames.get();
                        for (pkgxx::pkgname const& name: latest_pkgnames) {
                            if (auto installed = installed_pkgnames.lower_bound(
                                    pkgxx::pkgname(name.base, pkgxx::pkgversion()));
                                installed != installed_pkgnames.end() &&
                                installed->base == name.base &&
                                !_deleted_pkgnames.count(name)) {

                                if (installed->version == name.version) {
                                    // The latest PKGNAME turned out to be
                                    // installed. Good, but that's not
                                    // enough if -B is given.
                                    if (_check_build_version) {
                                        auto const latest_build_version    = fetch_build_version(name, path);
                                        auto const installed_build_version =
                                            pkgxx::build_version::from_installed(_PKG_INFO.get(), *installed);
                                        assert(installed_build_version.has_value());

                                        if (latest_build_version.has_value()) {
                                            if (latest_build_version == installed_build_version) {
                                                atomic_verbose(
                                                    [&](auto& out) {
                                                        out << path << " - " << name << " OK" << std::endl;
                                                    });
                                            }
                                            else {
                                                atomic_msg(
                                                    [&](auto& out) {
                                                        out << path << " - " << name << " build_version mismatch" << std::endl;
                                                    });
                                                atomic_verbose(
                                                    [&](auto& out) {
                                                        out << "--current--"                   << std::endl
                                                            << latest_build_version.value()
                                                            << "--installed--"                 << std::endl
                                                            << installed_build_version.value()
                                                            << "----"                          << std::endl
                                                            << std::endl;
                                                    });
                                                res.lock()->MISMATCH_TODO.emplace(*installed, path);
                                            }
                                        }
                                        else {
                                            atomic_msg(
                                                [&](auto& out) {
                                                    out << path << " - " << name << " build_version missing" << std::endl;
                                                });
                                        }
                                    }
                                    else {
                                        atomic_verbose(
                                            [&](auto& out) {
                                                out << path << " - " << name << " OK" << std::endl;
                                            });
                                    }
                                }
                                else if (installed->version < name.version) {
                                    // We have an older version installed.
                                    atomic_msg(
                                        [&](auto& out) {
                                            out << path << " - " << *installed << " < " << name
                                                << (is_binary_available(name) ? " (has binary package)" : "")
                                                << std::endl;
                                        });
                                    res.lock()->MISMATCH_TODO.emplace(*installed, path);
                                }
                                else {
                                    // We have a newer version installed
                                    // but how can that happen?
                                    if (_check_build_version) {
                                        atomic_msg(
                                            [&](auto& out) {
                                                out << path << " - " << *installed << " > " << name
                                                    << (is_binary_available(name) ? " (has binary package)" : "")
                                                    << std::endl;
                                            });
                                        res.lock()->MISMATCH_TODO.emplace(*installed, path);
                                    }
                                    else {
                                        atomic_msg(
                                            [&](auto& out) {
                                                out << path << " - " << *installed << " > " << name << " - ignoring"
                                                    << (is_binary_available(name) ? " (has binary package)" : "")
                                                    << std::endl;
                                            });
                                    }
                                }
                            }
                            else {
                                atomic_msg(
                                    [&](auto& out) {
                                        out << path << " - " << name << " missing"
                                            << (is_binary_available(name) ? " (has binary package)" : "")
                                            << std::endl;
                                    });
                                res.lock()->MISSING_TODO.emplace(name, path);
                            }
                        }
                        progress();
                    });
            }
        }
        done();

        // The nursery has to be destroyed before this std::move() happens,
        // otherwise we would return an empty result.
        return std::move(*res.lock());
    }

    checker_base::result
    checker_base::run() const {
        std::set<pkgxx::pkgpath> pkgpaths;
        for (auto const& [name, vars]: _installed_pkg_summary.get()) {
            if (!_deleted_pkgnames.count(name)) {
                pkgpaths.insert(vars.PKGPATH);
            }
        }
        return run(pkgpaths);
    }

    void
    checker_base::mark_as_deleted(pkgxx::pkgname const& name) {
        _deleted_pkgnames.insert(name);
    }

    source_checker_base::source_checker_base(
        std::shared_future<std::filesystem::path> const& PKGSRCDIR)
        : _PKGSRCDIR(PKGSRCDIR)
        , _installed_pkgpaths_with_pkgnames(
            std::async(
                std::launch::deferred,
                [this]() {
                    std::map<pkgxx::pkgpath, std::set<pkgxx::pkgname>> ret;
                    for (auto const& [name, vars]: _installed_pkg_summary.get()) {
                        ret[vars.PKGPATH].insert(name);
                    }
                    return ret;
                }).share()) {}

    std::set<pkgxx::pkgname>
    source_checker_base::find_latest_pkgnames(pkgxx::pkgpath const& path) const {
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
        if (!fs::exists(_PKGSRCDIR.get() / path / "Makefile")) {
            atomic_warn(
                [&](auto& out) {
                    out << "No " << path << "/Makefile - package moved or obsolete?" << std::endl;
                });
            return {};
        }

        auto const default_pkgname =
            pkgxx::extract_pkgmk_var<pkgxx::pkgname>(_PKGSRCDIR.get() / path, "PKGNAME");
        if (!default_pkgname) {
            fatal(
                [&](auto& out) {
                    out << "Unable to extract PKGNAME for " << path << std::endl;
                });
            std::terminate(); // Should not reach here.
        }

        // We search non-default PKGNAMEs only when -u or -r is given,
        // otherwise -a would install every single PKGNAME that the PKGPATH
        // provides.
        std::set<pkgxx::pkgname> pkgnames = {
            pkgxx::pkgname(*default_pkgname)
        };
        if (_update || _delete_mismatched) {
            auto const& pm = _installed_pkgpaths_with_pkgnames.get();
            if (auto installed_pkgnames = pm.find(path); installed_pkgnames != pm.end()) {
                for (auto const& installed_pkgname: installed_pkgnames->second) {
                    if (_deleted_pkgnames.count(installed_pkgname)) {
                        continue;
                    }
                    else if (installed_pkgname.base != default_pkgname->base) {
                        // We found a non-default PKGBASE but spawning
                        // make(1) takes seriously long. It's really
                        // tempting to cheat by making up a PKGNAME by
                        // combining it with the already known PKGVERSION,
                        // but we can't. This is because previously
                        // supported Python versions (or Ruby, or Lua, or
                        // whatever) might not be supported anymore, and we
                        // must treat it like a removed package in that
                        // case.
                        auto const alternative_pkgname =
                            pkgxx::extract_pkgmk_var<pkgxx::pkgname>(
                                _PKGSRCDIR.get() / path,
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

    std::optional<pkgxx::build_version>
    source_checker_base::fetch_build_version(pkgxx::pkgname const&, pkgxx::pkgpath const& path) const {
        return pkgxx::build_version::from_source(_PKGSRCDIR.get(), path);
    }

    binary_checker_base::binary_checker_base(
        std::shared_future<std::filesystem::path> const& PACKAGES,
        std::shared_future<std::string> const& PKG_SUFX,
        std::shared_future<pkgxx::summary> const& bin_pkg_summary)
        : _PACKAGES(PACKAGES)
        , _PKG_SUFX(PKG_SUFX)
        , _bin_pkg_summary(bin_pkg_summary)
        , _bin_pkg_map(
            std::async(
                std::launch::deferred,
                [this]() {
                    return pkgxx::pkgmap(_bin_pkg_summary.get());
                }).share()) {}

    std::set<pkgxx::pkgname>
    binary_checker_base::find_latest_pkgnames(pkgxx::pkgpath const& path) const {
        // We can enumerate every possible PKGNAME a PKGPATH can provide
        // just by querying the binary package summary. However, we cannot
        // know which one is the default without consulting the
        // source. This is problematic when -a is given. The best
        // workaround we can do is to sort the pkgbases and pick the last
        // one, but this is of course not guaranteed to pick the correct
        // package.
        auto const& pm = _bin_pkg_map.get();
        if (auto pkgbases = pm.find(path); pkgbases != pm.end()) {
            std::set<pkgxx::pkgname> pkgnames;
            if (_add_missing) {
                // Guess the default pkgbase. This may be inaccurate.
                auto guessed_default = pkgbases->second.rbegin();
                assert(guessed_default != pkgbases->second.rend());

                auto latest = guessed_default->second.rbegin();
                assert(latest != guessed_default->second.rend());

                pkgnames.insert(latest->first);
            }
            if (_update || _delete_mismatched) {
                // We need to enumerate only PKGBASEs that are already
                // installed, otherwise -a would install every single
                // package that the PKGPATH provides.
                auto const& installed_pkgnames = _installed_pkgnames.get();
                for (auto const& [base, sum]: pkgbases->second) {
                    if (auto installed = installed_pkgnames.lower_bound(
                            pkgxx::pkgname(base, pkgxx::pkgversion()));
                        installed != installed_pkgnames.end() &&
                        installed->base == base &&
                        !_deleted_pkgnames.count(*installed)) {

                        auto latest = sum.rbegin();
                        assert(latest != sum.rend());

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

    std::optional<pkgxx::build_version>
    binary_checker_base::fetch_build_version(pkgxx::pkgname const& name, pkgxx::pkgpath const&) const {
        if (auto const file = binary_package_file_of(name); file) {
            return pkgxx::build_version::from_binary(_PKG_INFO.get(), *file);
        }
        else {
            return std::nullopt;
        }
    }

    std::optional<std::filesystem::path>
    binary_checker_base::binary_package_file_of(pkgxx::pkgname const& name) const {
        auto const& sum = _bin_pkg_summary.get();

        if (auto it = sum.find(name); it != sum.end()) {
            if (it->second.FILE_NAME) {
                return _PACKAGES.get() / *(it->second.FILE_NAME);
            }
            else {
                auto file = _PACKAGES.get() / name.string();
                file += _PKG_SUFX.get();
                return file;
            }
        }
        else {
            return {};
        }
    }
}
