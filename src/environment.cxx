#include <cerrno>
#include <filesystem>
#include <initializer_list>
#include <stdlib.h>
#include <utility>
#include <vector>

#include "environment.hxx"
#include "makevars.hxx"
#include "message.hxx"

namespace fs = std::filesystem;

namespace {
    std::string
    cgetenv(std::string const& name) {
        char const* const value = getenv(name.c_str());
        return value ? std::string(value) : std::string();
    }

    struct makefile_env {
        fs::path    PACKAGES;
        std::string PKG_SUFX;
        fs::path    PKGCHK_CONF;
        std::string PKGCHK_NOTAGS;
        std::string PKGCHK_TAGS;
        fs::path    PKGCHK_UPDATE_CONF;
    };
}

namespace pkg_chk {
    environment::environment(pkg_chk::options const& opts) {
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
                        "/usr/pkgsrc",
                        ".",
                        "..",
                        "../.."
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
                    "PKG_SUFX",
                    "PKGCHK_CONF",
                    "PKGCHK_NOTAGS",
                    "PKGCHK_TAGS",
                    "PKGCHK_UPDATE_CONF"
                };
                std::map<std::string, std::string> value_of;
                auto const pkgdir = PKGSRCDIR.get() / "pkgtools/pkg_chk"; // Any package will do.
                if (fs::is_directory(pkgdir)) {
                    value_of = extract_pkgmk_vars(opts, pkgdir, vars);
                }
                else if (MAKECONF.get() != "/dev/null") {
                    value_of = extract_mkconf_vars(opts, MAKECONF.get(), vars);
                }
                _menv.PACKAGES           = value_of["PACKAGES"          ];
                _menv.PKG_SUFX           = value_of["PKG_SUFX"          ];
                _menv.PKGCHK_CONF        = value_of["PKGCHK_CONF"       ];
                _menv.PKGCHK_NOTAGS      = value_of["PKGCHK_NOTAGS"     ];
                _menv.PKGCHK_TAGS        = value_of["PKGCHK_TAGS"       ];
                _menv.PKGCHK_UPDATE_CONF = value_of["PKGCHK_UPDATE_CONF"];

                return _menv;
            }).share();

        PACKAGES = std::async(
            std::launch::deferred,
            [menv]() {
                return menv.get().PACKAGES;
            }).share();
    }
}
