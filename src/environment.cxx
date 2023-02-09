#include <cerrno>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <system_error>
#include <vector>

#include "environment.hxx"
#include "makevars.hxx"
#include "message.hxx"

namespace {
    std::string
    cgetenv(std::string const& name) {
        char const* const value = getenv(name.c_str());
        return value ? std::string(value) : std::string();
    }

    bool
    exists(std::string const& path) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            return true;
        }
        else if (errno == ENOENT) {
            return false;
        }
        else {
            throw std::system_error(errno, std::generic_category(), "stat: " + path);
        }
    }

    std::string
    absolute(std::string const& path) {
        char* const abs = realpath(path.c_str(), NULL);
        if (abs) {
            return abs;
        }
        else {
            throw std::system_error(errno, std::generic_category(), "realpath: " + path);
        }
    }
}

namespace pkg_chk {
    environment::environment(pkg_chk::options const& opts) {
        // Hide PKG_PATH to avoid breakage in 'make' calls.
        PKG_PATH = cgetenv("PKG_PATH");
        unsetenv("PKG_PATH");

        // MAKECONF
        MAKECONF = cgetenv("MAKECONF");
        if (MAKECONF.empty()) {
            std::vector<std::string> const mkconf_candidates = {
                CFG_MAKECONF,
                CFG_PREFIX "/etc/mk.conf",
                "/etc/mk.conf"
            };
            for (auto const mkconf: mkconf_candidates) {
                if (exists(mkconf)) {
                    MAKECONF = mkconf;
                    break;
                }
            }
        }
        if (MAKECONF.empty()) {
            MAKECONF = "/dev/null";
        }
        verbose_var(opts, "MAKECONF", MAKECONF);

        // PKGSRCDIR and LOCALBASE
        PKGSRCDIR = cgetenv("PKGSRCDIR");
        LOCALBASE = cgetenv("LOCALBASE");
        if (PKGSRCDIR.empty()) {
            std::vector<std::string> vars = {
                "PKGSRCDIR"
            };
            if (LOCALBASE.empty()) {
                vars.push_back("LOCALBASE");
            }
            auto value_of = extract_mk_vars(opts, MAKECONF, vars);
            PKGSRCDIR = value_of["PKGSRCDIR"];
            if (LOCALBASE.empty()) {
                LOCALBASE = value_of["LOCALBASE"];
            }
        }
        if (PKGSRCDIR.empty()) {
            // We couldn't extract PKGSRCDIR from mk.conf.
            std::vector<std::string> const pkgsrcdir_candidates = {
                LOCALBASE + "/pkgsrc",
                "/usr/pkgsrc",
                ".",
                "..",
                "../.."
            };
            for (auto const pkgsrcdir: pkgsrcdir_candidates) {
                std::string const path = pkgsrcdir + "/mk/bsd.pkg.mk";
                if (exists(path)) {
                    PKGSRCDIR = absolute(pkgsrcdir);
                    break;
                }
            }
            verbose_var(opts, "PKGSRCDIR", PKGSRCDIR);
        }
    }
}
