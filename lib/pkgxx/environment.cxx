#include <exception>
#include <stdlib.h>

#include "config.h"
#include "environment.hxx"
#include "makevars.hxx"

namespace fs = std::filesystem;

namespace pkgxx {
    std::optional<std::string>
    cgetenv(std::string const& name) {
        if (char const* const value = getenv(name.c_str()); value) {
            return value;
        }
        else {
            return std::nullopt;
        }
    }

    environment::environment() {
        // Hide PKG_PATH to avoid breakage in 'make' calls.
        {
            fs::path const vPKG_PATH = cgetenv("PKG_PATH").value_or("");
            unsetenv("PKG_PATH");
            PKG_PATH = std::async(
                std::launch::deferred,
                [&]() {
                    // We need to do this asynchronously only because
                    // verbose_var() is a pure virtual method and we are in
                    // the constructor.
                    verbose_var("PKG_PATH", vPKG_PATH.string());
                    return vPKG_PATH;
                });
        }

        // MAKECONF
        MAKECONF = std::async(
            std::launch::deferred,
            [&]() {
                //
                // Lazy evaluation with std::async(std::launch::deferred).
                //
                fs::path vMAKECONF = cgetenv("MAKECONF").value_or("");
                if (vMAKECONF.empty()) {
                    std::initializer_list<fs::path> const candidates = {
#if defined(CFG_MAKECONF)
                        CFG_MAKECONF,
#endif
                        CFG_PREFIX "/etc/mk.conf",
                        "/etc/mk.conf"
                    };
                    for (auto const &mkconf: candidates) {
                        if (fs::exists(mkconf)) {
                            vMAKECONF = mkconf;
                            break;
                        }
                    }
                }
                if (vMAKECONF.empty()) {
                    vMAKECONF = "/dev/null";
                }
                verbose_var("MAKECONF", vMAKECONF.string());
                return vMAKECONF;
            }).share();

        // PKGSRCDIR
        PKGSRCDIR = std::async(
            std::launch::deferred,
            [&]() {
                fs::path vPKGSRCDIR = cgetenv("PKGSRCDIR").value_or("");
                fs::path vLOCALBASE = cgetenv("LOCALBASE").value_or("");

                if (vPKGSRCDIR.empty()) {
                    std::vector<std::string> vars = {
                        "PKGSRCDIR"
                    };
                    if (vLOCALBASE.empty()) {
                        vars.push_back("LOCALBASE");
                    }

                    auto value_of = pkgxx::extract_mkconf_vars(MAKECONF.get(), vars).value();
                    for (auto const& [var, value]: value_of) {
                        verbose_var(var, value);
                    }

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
                    for (auto const &pkgsrcdir: candidates) {
                        if (fs::exists(pkgsrcdir / "mk/bsd.pkg.mk")) {
                            vPKGSRCDIR = fs::absolute(pkgsrcdir);
                            break;
                        }
                    }
                    verbose_var("PKGSRCDIR", vPKGSRCDIR.string());
                }
                return vPKGSRCDIR;
            }).share();
    }
}
