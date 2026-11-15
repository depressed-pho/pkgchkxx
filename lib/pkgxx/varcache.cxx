#include <exception>
#include <unistd.h>

#include "environment.hxx"
#include "makevars.hxx"
#include "tty.hxx"
#include "varcache.hxx"

namespace pkgxx {
    void
    varcache::default_vars() & {
        if (_populated) {
            throw std::logic_error("varcache has already been populated");
        }

        // mk/bsd.prefs.mk
        add("HOST_MACHINE_ARCH");
        add("_UNAME_V"); // Only makes sense for SunOS
        add("_PKGSRCDIR");
        add("_HOSTNAME");

        // mk/compiler/clang.mk
        add("CC_VERSION");

        // mk/compiler/gcc.mk
        add("_GCC_VERSION");
        // _GCC_PREFIX is a candidate as well but it doesn't look safe to
        // cache.

        // mk/install/bsd.install-vars.mk
        add("_MANCOMPRESSED");

        // mk/endian.mk
        add("MACHINE_ENDIAN");

        // mk/platform/Darwin.mk
        add("OSX_VERSION");
        add("OSX_SDK_PATH");

        // mk/pkgformat/pkg/pkgformat-vars.mk
        add("PKGTOOLS_VERSION");

        // mk/unprivileged.mk
        add("UNPRIVILEGED_USER");
        add("UNPRIVILEGED_GROUP");
        add("UNPRIVILEGED_GROUPS");
    }

    void
    varcache::add(std::string_view const& var) & {
        if (_populated) {
            throw std::logic_error("varcache has already been populated");
        }
        _to_cache.emplace(var);
    }

    void
    varcache::populate() {
        if (_populated) {
            return;
        }

        auto const vars =
            pkgxx::extract_pkgmk_vars(
                _pkgsrcdir / "pkgtools" / "pkg_install", // Any packages will do.
                std::vector<std::string>(_to_cache.begin(), _to_cache.end()));
        if (!vars) {
            throw std::runtime_error("pkgtools/pkg_install does not exist");
        }

        for (auto const& [name, value]: *vars) {
            pkgxx::csetenv(name, value);
        }

        if (_show_msg) {
            maybe_ttystream out{STDERR_FILENO};
            out.push_style(
                tty::dull_colour(tty::blue),
                pkgxx::ttystreambuf_base::how::no_combine);

            if (!vars->empty()) {
                out << "Cached variables:" << std::endl;
                for (auto const& [name, value]: *vars) {
                    out << "  * " << name << " = " << value << std::endl;
                }
            }
            else {
                out << "Cached no pkgsrc variables." << std::endl;
            }
        }
    }
}
