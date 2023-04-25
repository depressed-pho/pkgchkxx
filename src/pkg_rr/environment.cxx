#include <pkgxx/makevars.hxx>

#include "config.h"
#include "environment.hxx"
#include "message.hxx"

namespace fs = std::filesystem;

namespace {
    struct makefile_env {
        std::string PKG_ADMIN;
        std::string PKG_INFO;
        std::string SU_CMD;
    };
}

namespace pkg_rr {
    environment::environment(pkg_rr::options const& opts)
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
                    fatal([this](auto& out) {
                              out << "Unable to locate PKGSRCDIR ("
                                  << (PKGSRCDIR.get().empty() ? "not set" : PKGSRCDIR.get())
                                  << ")" << std::endl;
                          });
                }
                std::vector<std::string> vars = {
                    "PKG_INFO",
                    "SU_CMD"
                };
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
                _menv.PKG_ADMIN = value_of["PKG_ADMIN"].empty() ? CFG_PKG_ADMIN : value_of["PKG_ADMIN"];
                _menv.PKG_INFO  = value_of["PKG_INFO" ].empty() ? CFG_PKG_INFO  : value_of["PKG_INFO" ];
                _menv.SU_CMD    = value_of["SU_CMD"   ];
                return _menv;
            }).share();
        PKG_ADMIN = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_ADMIN; }).share();
        PKG_INFO  = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_INFO;  }).share();
        SU_CMD    = std::async(std::launch::deferred, [menv]() { return menv.get().SU_CMD;    }).share();
    }
}
