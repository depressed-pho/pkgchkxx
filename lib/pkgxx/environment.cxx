#include <exception>
#include <stdlib.h>

#include "config.h"
#include "environment.hxx"
#include "makevars.hxx"

namespace fs = std::filesystem;

namespace pkgxx {
    std::string
    cgetenv(std::string const& name) {
        char const* const value = getenv(name.c_str());
        return value ? value : "";
    }

    environment::environment(
        std::function<
                void (std::string_view const&, std::string_view const&)
                > const& var_logger)
        : _var_logger(var_logger) {

        // Hide PKG_PATH to avoid breakage in 'make' calls.
        {
            std::string const path = cgetenv("PKG_PATH");
            std::promise<fs::path> p;
            p.set_value(path);
            PKG_PATH = p.get_future();
            unsetenv("PKG_PATH");
            _var_logger("PKG_PATH", path);
        }

        // MAKECONF
        MAKECONF = std::async(
            std::launch::deferred,
            [&]() {
                //
                // Lazy evaluation with std::async(std::launch::deferred).
                //
                fs::path vMAKECONF = cgetenv("MAKECONF");
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
                _var_logger("MAKECONF", vMAKECONF.string());
                return vMAKECONF;
            }).share();

        // PKGSRCDIR
        PKGSRCDIR = std::async(
            std::launch::deferred,
            [&]() {
                fs::path vPKGSRCDIR = cgetenv("PKGSRCDIR");
                fs::path vLOCALBASE = cgetenv("LOCALBASE");

                if (vPKGSRCDIR.empty()) {
                    std::vector<std::string> vars = {
                        "PKGSRCDIR"
                    };
                    if (vLOCALBASE.empty()) {
                        vars.push_back("LOCALBASE");
                    }

                    auto value_of = pkgxx::extract_mkconf_vars(MAKECONF.get(), vars).value();
                    for (auto const& [var, value]: value_of) {
                        _var_logger(var, value);
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
                    _var_logger("PKGSRCDIR", vPKGSRCDIR.string());
                }
                return vPKGSRCDIR;
            }).share();
    }
}
