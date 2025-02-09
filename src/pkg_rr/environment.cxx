#include "config.h"

#include <pkgxx/makevars.hxx>
#include <unistd.h>

#include "environment.hxx"
#include "message.hxx"

namespace fs = std::filesystem;
namespace tty = pkgxx::tty;

namespace {
    struct makefile_env {
        std::optional<pkgxx::pkgbase> FETCH_USING;
        std::string PKG_ADMIN;
        std::string PKG_INFO;
        std::string SU_CMD;
    };
}

namespace pkg_rr {
    environment::environment(pkg_rr::options const& opts)
        : opts(opts)
        , _cerr(std::make_shared<pkgxx::maybe_ttystream>(STDERR_FILENO)) {

        // Now we have PKGSRCDIR, use it to collect values that can only be
        // obtained from pkgsrc Makefiles.
        std::shared_future<makefile_env> const menv = std::async(
            std::launch::deferred,
            [this]() {
                makefile_env _menv;

                if (!fs::is_directory(PKGSRCDIR.get())) {
                    fatal([this](auto& out) {
                              out << "Unable to locate PKGSRCDIR ("
                                  << (PKGSRCDIR.get().empty() ? "not set" : PKGSRCDIR.get())
                                  << ")" << std::endl;
                          });
                }
                std::vector<std::string> vars = {
                    "FETCH_USING",
                    "PKG_ADMIN",
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
                    verbose_var(var, value);
                }
                _menv.FETCH_USING = value_of["FETCH_USING"].empty() ? std::nullopt  : std::make_optional(value_of["FETCH_USING"]);
                _menv.PKG_ADMIN   = value_of["PKG_ADMIN"  ].empty() ? CFG_PKG_ADMIN : value_of["PKG_ADMIN"];
                _menv.PKG_INFO    = value_of["PKG_INFO"   ].empty() ? CFG_PKG_INFO  : value_of["PKG_INFO" ];
                _menv.SU_CMD      = value_of["SU_CMD"     ];
                return _menv;
            }).share();
        FETCH_USING = std::async(std::launch::deferred, [menv]() { return menv.get().FETCH_USING; }).share();
        PKG_ADMIN   = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_ADMIN;   }).share();
        PKG_INFO    = std::async(std::launch::deferred, [menv]() { return menv.get().PKG_INFO;    }).share();
        SU_CMD      = std::async(std::launch::deferred, [menv]() { return menv.get().SU_CMD;      }).share();
    }

    pkgxx::maybe_tty_osyncstream
    environment::raw_msg() const {
        return pkgxx::maybe_tty_osyncstream(*_cerr);
    }

    msgstream
    environment::verbose(unsigned level) const {
        return verbose(*_cerr, level);
    }

    msgstream
    environment::verbose(pkgxx::ttystream_base& out, unsigned level) const {
        if (opts.verbose >= level) {
            return msgstream(
                out,
                tty::dull_colour(tty::blue));
        }
        else {
            return msgstream();
        }
    }

    void
    environment::verbose_var(
        std::string_view const& var,
        std::string_view const& value) const {

        verbose_var(var, value, *_cerr);
    }

    void
    environment::verbose_var(
        std::string_view const& var,
        std::string_view const& value,
        pkgxx::ttystream_base& out) const {

        verbose(out, 2)
            << "Variable: " << var << " = " << (value.empty() ? "(empty)" : value) << std::endl;
    }

    msgstream
    environment::msg() const {
        return msg(*_cerr);
    }

    msgstream
    environment::msg(pkgxx::ttystream_base& out) const {
        return msgstream(out, tty::dull_colour(tty::green));
    }

    msgstream
    environment::warn() const {
        return warn(*_cerr);
    }

    msgstream
    environment::warn(pkgxx::ttystream_base& out) const {
        auto out_ = msgstream(out, tty::bold + tty::colour(tty::yellow));
        out_ << "WARNING: ";
        return out_;
    }

    msgstream
    environment::error() const {
        return error(*_cerr);
    }

    msgstream
    environment::error(pkgxx::ttystream_base& out) const {
        auto out_ = msgstream(out, tty::bold + tty::colour(tty::red));
        out_ << "*** ";
        return out_;
    }

    [[noreturn]] void
    environment::fatal(std::function<void (msgstream&)> const& f) const {
        fatal(f, *_cerr);
    }

    [[noreturn]] void
    environment::fatal(
        std::function<void (msgstream&)> const& f,
        pkgxx::ttystream_base& out) const {

        auto out_ = error(out);
        f(out_);
        std::exit(1);
    }
}
