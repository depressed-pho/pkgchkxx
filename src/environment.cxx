#include <cerrno>
#include <filesystem>
#include <initializer_list>
#include <stdlib.h>
#include <sys/utsname.h>
#include <system_error>
#include <utility>
#include <unistd.h>
#include <vector>

#include "environment.hxx"
#include "makevars.hxx"
#include "message.hxx"

namespace fs = std::filesystem;

namespace {
    std::string
    cgetenv(std::string const& name) {
        char const* const value = getenv(name.c_str());
        return value ? value : "";
    }

    struct utsname
    cuname() {
        struct utsname un;
        if (uname(&un) != 0) {
            throw std::system_error(errno, std::generic_category(), "uname");
        }
        return un;
    }

    fs::path
    url_safe_absolute(fs::path const& path) {
        if (path.string().find("://")) {
            // Looks like an absolute URI.
            return path;
        }
        else {
            return fs::absolute(path);
        }
    }

    struct makefile_env {
        fs::path        PACKAGES;
        fs::path        PKG_DBDIR;
        std::string     PKG_INFO;
        std::string     PKG_SUFX;
        fs::path        PKG_SYSCONFDIR;
        fs::path        PKGCHK_CONF;
        pkg_chk::tagset PKGCHK_NOTAGS;
        pkg_chk::tagset PKGCHK_TAGS;
        fs::path        PKGCHK_UPDATE_CONF;
        std::string     SU_CMD;
    };
}

namespace pkg_chk {
    environment::environment(pkg_chk::options const& opts) {
        using namespace std::literals;

        // Hide PKG_PATH to avoid breakage in 'make' calls.
        {
            std::promise<fs::path> p;
            p.set_value(cgetenv("PKG_PATH"));
            PKG_PATH = p.get_future();
            unsetenv("PKG_PATH");
        }

        // MAKECONF
        MAKECONF = std::async(
            std::launch::deferred,
            [&opts]() {
                fs::path vMAKECONF = cgetenv("MAKECONF");
                if (vMAKECONF.empty()) {
                    std::initializer_list<fs::path> const candidates = {
                        CFG_MAKECONF,
                        CFG_PREFIX "/etc/mk.conf",
                        "/etc/mk.conf"
                    };
                    for (auto const mkconf: candidates) {
                        if (fs::exists(mkconf)) {
                            vMAKECONF = mkconf;
                            break;
                        }
                    }
                }
                if (vMAKECONF.empty()) {
                    vMAKECONF = "/dev/null";
                }
                verbose_var(opts, "MAKECONF", vMAKECONF);
                return vMAKECONF;
            }).share();

        // PKGSRCDIR
        PKGSRCDIR = std::async(
            std::launch::deferred,
            [this, &opts]() {
                fs::path vPKGSRCDIR = cgetenv("PKGSRCDIR");
                fs::path vLOCALBASE = cgetenv("LOCALBASE");

                if (vPKGSRCDIR.empty()) {
                    std::vector<std::string> vars = {
                        "PKGSRCDIR"
                    };
                    if (vLOCALBASE.empty()) {
                        vars.push_back("LOCALBASE");
                    }

                    auto value_of = extract_mkconf_vars(opts, MAKECONF.get(), vars);
                    vPKGSRCDIR = value_of["PKGSRCDIR"];
                    if (vLOCALBASE.empty()) {
                        vLOCALBASE = value_of["LOCALBASE"];
                    }
                }
                if (vPKGSRCDIR.empty()) {
                    // We couldn't extract PKGSRCDIR from mk.conf.
                    std::initializer_list<fs::path> const candidates = {
                        vLOCALBASE / "pkgsrc",
                        ".",
                        "..",
                        "../..",
                        "/usr/pkgsrc"
                    };
                    for (auto const pkgsrcdir: candidates) {
                        if (fs::exists(pkgsrcdir / "mk/bsd.pkg.mk")) {
                            vPKGSRCDIR = fs::absolute(pkgsrcdir);
                            break;
                        }
                    }
                    verbose_var(opts, "PKGSRCDIR", vPKGSRCDIR);
                }
                return vPKGSRCDIR;
            }).share();

        // Now we have PKGSRCDIR, use it to collect values that can only be
        // obtained from pkgsrc Makefiles.
        std::shared_future<makefile_env> menv = std::async(
            std::launch::deferred,
            [this, &opts]() {
                makefile_env _menv;

                if (!fs::is_directory(PKGSRCDIR.get())) {
                    fatal(opts, [this](logger& out) {
                                    out << "Unable to locate PKGSRCDIR ("
                                        << (PKGSRCDIR.get().empty() ? "not set" : PKGSRCDIR.get())
                                        << ")" << std::endl;
                                });
                }
                std::vector<std::string> vars = {
                    "PACKAGES",
                    "PKG_DBDIR",
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
                auto const pkgdir = PKGSRCDIR.get() / "pkgtools/pkg_chk"; // Any package will do.
                if (fs::is_directory(pkgdir)) {
                    value_of = extract_pkgmk_vars(opts, pkgdir, vars);
                }
                else if (MAKECONF.get() != "/dev/null") {
                    value_of = extract_mkconf_vars(opts, MAKECONF.get(), vars);
                }
                _menv.PACKAGES           =
                    opts.bin_pkg_path.empty()
                    ? fs::path(value_of["PACKAGES"])
                    : url_safe_absolute(opts.bin_pkg_path);
                _menv.PKG_DBDIR          = value_of["PKG_DBDIR"         ];
                _menv.PKG_INFO           = value_of["PKG_INFO"          ];
                _menv.PKG_SUFX           = value_of["PKG_SUFX"          ];
                _menv.PKG_SYSCONFDIR     = value_of["PKG_SYSCONFDIR"    ];
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
                if (!fs::is_directory(_menv.PKG_DBDIR)) {
                    fatal(opts, [&_menv](logger& out) {
                                    out << "Unable to locate PKG_DBDIR ("
                                        << (_menv.PKG_DBDIR.empty() ? "not set" : _menv.PKG_DBDIR)
                                        << ")" << std::endl;
                                });
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
        PKG_DBDIR          = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_DBDIR;          }).share();
        PKG_INFO           = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_INFO;           }).share();
        PKG_SUFX           = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_SUFX;           }).share();
        PKGCHK_CONF        = std::async(std::launch::deferred, [menv]() { return menv.get().PKGCHK_CONF;        }).share();
        PKGCHK_NOTAGS      = std::async(std::launch::deferred, [menv]() { return menv.get().PKGCHK_NOTAGS;      }).share();
        PKGCHK_TAGS        = std::async(std::launch::deferred, [menv]() { return menv.get().PKGCHK_TAGS;        }).share();
        PKGCHK_UPDATE_CONF = std::async(std::launch::deferred, [menv]() { return menv.get().PKGCHK_UPDATE_CONF; }).share();
        SU_CMD             = std::async(std::launch::deferred, [menv]() { return menv.get().SU_CMD;             }).share();
    }
}
