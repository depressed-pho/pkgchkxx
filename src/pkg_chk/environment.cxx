#include <cerrno>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <sys/utsname.h>
#include <system_error>
#include <utility>
#include <unistd.h>
#include <vector>

#include <pkgxx/harness.hxx>
#include <pkgxx/makevars.hxx>
#include <pkgxx/pkgdb.hxx>

#include "config.h"
#include "environment.hxx"
#include "message.hxx"

using namespace std::literals;
namespace fs = std::filesystem;

namespace {
    utsname
    cuname() {
        utsname un;
        if (uname(&un) != 0) {
            throw std::system_error(errno, std::generic_category(), "uname");
        }
        return un;
    }

    fs::path
    url_safe_absolute(fs::path const& path) {
        if (path.string().find("://") != std::string::npos) {
            // Looks like an absolute URI.
            return path;
        }
        else {
            return fs::absolute(path);
        }
    }

    struct makefile_env {
        fs::path        PACKAGES;
        std::string     PKG_ADD;
        std::string     PKG_ADMIN;
        std::string     PKG_DELETE;
        std::string     PKG_INFO;
        std::string     PKG_SUFX;
        fs::path        PKG_SYSCONFDIR;
        fs::path        PKGCHK_CONF;
        pkg_chk::tagset PKGCHK_NOTAGS;
        pkg_chk::tagset PKGCHK_TAGS;
        fs::path        PKGCHK_UPDATE_CONF;
        std::string     SU_CMD;
    };

    struct platform_env {
        std::string OPSYS;
        std::string OS_VERSION;
        std::string MACHINE_ARCH;
    };

    struct tags_env {
        pkg_chk::tagset included_tags;
        pkg_chk::tagset excluded_tags;
    };
}

namespace pkg_chk {
    environment::environment(pkg_chk::options const& opts)
        : pkgxx::environment(
            [&](auto&& var, auto&& value) {
                verbose_var(
                    opts,
                    std::forward<decltype(var)>(var),
                    std::forward<decltype(value)>(value));
            }) {

        // Now we have PKGSRCDIR, use it to collect values that can only be
        // obtained from pkgsrc Makefiles.
        std::shared_future<makefile_env> const menv = std::async(
            std::launch::deferred,
            [this, &opts]() {
                makefile_env _menv;

                if (!fs::is_directory(PKGSRCDIR.get())) {
                    fatal(opts, [this](auto& out) {
                                    out << "Unable to locate PKGSRCDIR ("
                                        << (PKGSRCDIR.get().empty() ? "not set" : PKGSRCDIR.get())
                                        << ")" << std::endl;
                                });
                }
                std::vector<std::string> vars = {
                    "PACKAGES",
                    "PKG_ADD",
                    "PKG_ADMIN",
                    "PKG_DELETE",
                    "PKG_INFO",
                    "PKG_SUFX",
                    "PKGCHK_NOTAGS",
                    "PKGCHK_TAGS",
                    "PKGCHK_UPDATE_CONF"
                };
                if (opts.pkgchk_conf_path.empty()) {
                    vars.push_back("PKGCHK_CONF_PATH");
                }
                if (opts.bin_pkg_path.empty()) {
                    vars.push_back("PACKAGES");
                }
                if (geteuid() != 0) {
                    vars.push_back("SU_CMD");
                }
                std::map<std::string, std::string> value_of;
                auto const pkgpath = PKGSRCDIR.get() / "pkgtools/pkg_install"; // Any package will do.
                if (fs::is_directory(pkgpath)) {
                    value_of = pkgxx::extract_pkgmk_vars(pkgpath, vars).value();
                }
                else if (MAKECONF.get() != "/dev/null") {
                    value_of = pkgxx::extract_mkconf_vars(MAKECONF.get(), vars).value();
                }
                for (auto const& [var, value]: value_of) {
                    verbose_var(opts, var, value);
                }
                _menv.PACKAGES           =
                    opts.bin_pkg_path.empty()
                    ? fs::path(value_of["PACKAGES"])
                    : url_safe_absolute(opts.bin_pkg_path);
                _menv.PKG_ADD            = value_of["PKG_ADD"   ].empty() ? CFG_PKG_ADD    : value_of["PKG_ADD"   ];
                _menv.PKG_ADMIN          = value_of["PKG_ADMIN" ].empty() ? CFG_PKG_ADMIN  : value_of["PKG_ADMIN" ];
                _menv.PKG_DELETE         = value_of["PKG_DELETE"].empty() ? CFG_PKG_DELETE : value_of["PKG_DELETE"];
                _menv.PKG_INFO           = value_of["PKG_INFO"  ].empty() ? CFG_PKG_INFO   : value_of["PKG_INFO"  ];
                _menv.PKG_SUFX           = value_of["PKG_SUFX"      ];
                _menv.PKG_SYSCONFDIR     = value_of["PKG_SYSCONFDIR"];
                _menv.PKGCHK_CONF        =
                    opts.pkgchk_conf_path.empty()
                    ? fs::path(value_of["PKGCHK_CONF"])
                    : url_safe_absolute(opts.pkgchk_conf_path);
                _menv.PKGCHK_NOTAGS      = value_of["PKGCHK_NOTAGS"     ];
                _menv.PKGCHK_TAGS        = value_of["PKGCHK_TAGS"       ];
                _menv.PKGCHK_UPDATE_CONF = value_of["PKGCHK_UPDATE_CONF"];
                _menv.SU_CMD             = value_of["SU_CMD"            ];

                if (_menv.PACKAGES.empty()) {
                    _menv.PACKAGES = PKGSRCDIR.get() / "packages";
                    verbose_var(opts, "PACKAGES", _menv.PACKAGES);
                }
                if (fs::is_directory(_menv.PACKAGES / "All")) {
                    _menv.PACKAGES /= "All";
                    verbose_var(opts, "PACKAGES", _menv.PACKAGES);
                }
                if (_menv.PKGCHK_CONF.empty()) {
                    // Check PKG_SYSCONFDIR then fall back to PKGSRCDIR.
                    if (fs::exists(_menv.PKG_SYSCONFDIR / "pkgchk.conf")) {
                        _menv.PKGCHK_CONF = _menv.PKG_SYSCONFDIR / "pkgchk.conf";
                    }
                    else {
                        _menv.PKGCHK_CONF = PKGSRCDIR.get() / "pkgchk.conf";
                    }
                    verbose_var(opts, "PKGCHK_CONF", _menv.PKGCHK_CONF);
                }
                if (_menv.PKGCHK_UPDATE_CONF.empty()) {
                    _menv.PKGCHK_UPDATE_CONF =
                        PKGSRCDIR.get() / ("pkgchk_update-"s + cuname().nodename + ".conf"s);
                    verbose_var(opts, "PKGCHK_UPDATE_CONF", _menv.PKGCHK_UPDATE_CONF);
                }

                return _menv;
            }).share();
        PACKAGES           = std::async(std::launch::deferred, [menv]() { return menv.get().PACKAGES;           }).share();
        PKG_ADD            = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_ADD;            }).share();
        PKG_ADMIN          = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_ADMIN;          }).share();
        PKG_DELETE         = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_DELETE;         }).share();
        PKG_INFO           = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_INFO;           }).share();
        PKG_SUFX           = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_SUFX;           }).share();
        PKGCHK_CONF        = std::async(std::launch::deferred, [menv]() { return menv.get().PKGCHK_CONF;        }).share();
        PKGCHK_NOTAGS      = std::async(std::launch::deferred, [menv]() { return menv.get().PKGCHK_NOTAGS;      }).share();
        PKGCHK_TAGS        = std::async(std::launch::deferred, [menv]() { return menv.get().PKGCHK_TAGS;        }).share();
        PKGCHK_UPDATE_CONF = std::async(std::launch::deferred, [menv]() { return menv.get().PKGCHK_UPDATE_CONF; }).share();
        SU_CMD             = std::async(std::launch::deferred, [menv]() { return menv.get().SU_CMD;             }).share();

        /* OPSYS, OS_VERSION, and MACHINE_ARCH should be retrieved from
         * Makefile, but if that's impossible we can fall back to
         * uname(3). */
        std::shared_future<platform_env> const penv = std::async(
            std::launch::deferred,
            [this, &opts]() {
                platform_env _penv;

                auto const pkgpath = PKGSRCDIR.get() / "pkgtools/pkg_chk"; // Any package will do.
                if (fs::is_directory(pkgpath)) {
                    std::vector<std::string> const vars = {
                        "OPSYS",
                        "OS_VERSION",
                        "MACHINE_ARCH"
                    };
                    auto value_of = pkgxx::extract_pkgmk_vars(pkgpath, vars).value();
                    for (auto const& [var, value]: value_of) {
                        verbose_var(opts, var, value);
                    }
                    _penv.OPSYS        = value_of["OPSYS"       ];
                    _penv.OS_VERSION   = value_of["OS_VERSION"  ];
                    _penv.MACHINE_ARCH = value_of["MACHINE_ARCH"];
                }
                else {
                    auto const un    = cuname();
                    _penv.OPSYS      = un.sysname;
                    _penv.OS_VERSION = un.release;
                    // (struct utsname)#machine isn't exactly what "uname
                    // -p" shows, but sysctl(7) hw.machine_arch isn't
                    // portable. We have to bite the bullet and spawn
                    // uname(1). Note that "uname -p" is a widely supported
                    // option but it isn't a POSIX standard either.
                    pkgxx::harness uname(CFG_UNAME, {CFG_UNAME, "-p"});
                    uname.cin().close();
                    std::getline(uname.cout(), _penv.MACHINE_ARCH);

                    verbose_var(opts, "OPSYS"       , _penv.OPSYS       );
                    verbose_var(opts, "OS_VERSION"  , _penv.OS_VERSION  );
                    verbose_var(opts, "MACHINE_ARCH", _penv.MACHINE_ARCH);
                }

                return _penv;
            }).share();
        OPSYS        = std::async(std::launch::deferred, [penv]() { return penv.get().OPSYS;        }).share();
        OS_VERSION   = std::async(std::launch::deferred, [penv]() { return penv.get().OS_VERSION;   }).share();
        MACHINE_ARCH = std::async(std::launch::deferred, [penv]() { return penv.get().MACHINE_ARCH; }).share();

        // The binary package summary is obtained by parsing a
        // pkg_summary(5) file or by scanning PACKAGES.
        bin_pkg_summary = std::async(
            std::launch::deferred,
            [this, &opts]() {
                auto m = msg(opts);
                auto v = verbose(opts);
                pkgxx::summary sum(m, v, opts.concurrency, PACKAGES.get(), PKG_INFO.get(), PKG_SUFX.get());
                verbose(opts) << "Binary packages: " << sum.size() << std::endl;
                return sum;
            }).share();
        bin_pkg_map = std::async(
            std::launch::deferred,
            [this]() {
                return pkgxx::pkgmap(bin_pkg_summary.get());
            }).share();

        installed_pkgnames = std::async(
            std::launch::deferred,
            [this, &opts]() {
                verbose(opts) << "Enumerate PKGNAME from installed packages" << std::endl;

                std::set<pkgxx::pkgname> pkgnames;
                for (auto& name: pkgxx::installed_pkgnames(PKG_INFO.get())) {
                    pkgnames.insert(std::move(name));
                }
                return pkgnames;
            }).share();
        installed_pkgpaths = std::async(
            std::launch::deferred,
            [this, &opts]() {
                verbose(opts) << "Enumerate PKGPATH from installed packages" << std::endl;

                pkgxx::harness pkg_info(pkgxx::shell, {pkgxx::shell, "-s", "--", "-aQ", "PKGPATH"});
                pkg_info.cin() << "exec " << PKG_INFO.get() << " \"$@\"" << std::endl;
                pkg_info.cin().close();

                std::set<pkgxx::pkgpath> pkgpaths;
                for (std::string line; std::getline(pkg_info.cout(), line); ) {
                    if (!line.empty()) {
                        pkgpaths.emplace(line);
                    }
                }
                return pkgpaths;
            }).share();

        // Tags are collected from the platform, options, and Makefile
        // variables.
        std::shared_future<tags_env> tenv = std::async(
            std::launch::deferred,
            [this, &opts]() {
                tags_env _tenv;

                // If '-U' contains a '*' then we need to unset TAGS and
                // PKGCHK_TAGS, but still pick up -D, and even package
                // specific -U options.
                if (!opts.remove_tags.count("*")) {
                    std::string const hostname = cuname().nodename;
                    _tenv.included_tags.insert({
                        hostname.substr(0, hostname.find('.')),
                        hostname,
                        OPSYS.get() + '-' + OS_VERSION.get() + '-' + MACHINE_ARCH.get(),
                        OPSYS.get() + '-' + OS_VERSION.get(),
                        OPSYS.get() + '-' + MACHINE_ARCH.get(),
                        OPSYS.get(),
                        OS_VERSION.get(),
                        MACHINE_ARCH.get()
                    });
                    _tenv.included_tags.insert(
                        PKGCHK_TAGS.get().begin(), PKGCHK_TAGS.get().end());

                    using namespace na::literals;
                    pkgxx::harness pkg_config(
                        CFG_PKG_CONFIG,
                        {CFG_PKG_CONFIG, "--exists", "x11"},
                        "env_mod"_na = [](auto& env) {
                            std::string const libdir = CFG_PKG_CONFIG_LIBDIR;
                            if (!libdir.empty()) {
                                env["PKG_CONFIG_LIBDIR"] = libdir;
                            }

                            std::string const path = CFG_PKG_CONFIG_PATH;
                            if (!path.empty()) {
                                env["PKG_CONFIG_PATH"] = path;
                            }
                        });
                    pkg_config.cin().close();
                    pkg_config.cout().close();
                    if (pkg_config.wait_exit().status == 0) {
                        _tenv.included_tags.emplace("x11");
                    }
                }
                _tenv.included_tags.insert(
                    opts.add_tags.begin(), opts.add_tags.end());

                _tenv.excluded_tags.insert(
                    opts.remove_tags.begin(), opts.remove_tags.end());
                _tenv.excluded_tags.insert(
                    PKGCHK_NOTAGS.get().begin(), PKGCHK_NOTAGS.get().end());

                verbose(opts) << "set   TAGS=" << _tenv.included_tags << std::endl;
                verbose(opts) << "unset TAGS=" << _tenv.excluded_tags << std::endl;
                return _tenv;
            }).share();
        included_tags = std::async(std::launch::deferred, [tenv]() { return tenv.get().included_tags; }).share();
        excluded_tags = std::async(std::launch::deferred, [tenv]() { return tenv.get().excluded_tags; }).share();
    }
}
