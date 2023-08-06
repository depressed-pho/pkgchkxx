#pragma once

#include <filesystem>
#include <future>
#include <set>
#include <string>

#include <pkgxx/environment.hxx>
#include <pkgxx/summary.hxx>

#include "options.hxx"
#include "tag.hxx"

namespace pkg_chk {
    /** Values from the environment such as various Makefiles. Most of such
     * values are very expensive to retrieve so they are lazily
     * evaluated.
     *
     * Objects of this class MUST NOT be shared by threads. Each thread
     * must have its own copy.
     */
    struct environment: public pkgxx::environment {
        environment(pkg_chk::options const& opts);

        bool
        is_binary_available(pkgxx::pkgname const& name) const {
            return bin_pkg_summary.get().count(name) > 0;
        }

        std::shared_future<std::string>           MACHINE_ARCH;
        std::shared_future<std::string>           OPSYS;
        std::shared_future<std::string>           OS_VERSION;
        std::shared_future<std::filesystem::path> PACKAGES;
        std::shared_future<std::string>           PKG_ADD;
        std::shared_future<std::string>           PKG_ADMIN;
        std::shared_future<std::string>           PKG_DELETE;
        std::shared_future<std::string>           PKG_INFO;
        std::shared_future<std::string>           PKG_SUFX;
        std::shared_future<std::filesystem::path> PKGCHK_CONF;
        std::shared_future<tagset>                PKGCHK_NOTAGS;
        std::shared_future<tagset>                PKGCHK_TAGS;
        std::shared_future<std::filesystem::path> PKGCHK_UPDATE_CONF;
        std::shared_future<std::string>           SU_CMD;

        std::shared_future<pkgxx::summary> bin_pkg_summary;
        std::shared_future<pkgxx::pkgmap>  bin_pkg_map;

        std::shared_future<std::set<pkgxx::pkgname>> installed_pkgnames; // Fastest to compute.
        std::shared_future<std::set<pkgxx::pkgpath>> installed_pkgpaths; // Moderately slow.

        std::shared_future<tagset>  included_tags;
        std::shared_future<tagset>  excluded_tags;
    };
}
