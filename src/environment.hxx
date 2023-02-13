#pragma once

#include <filesystem>
#include <future>
#include <set>
#include <string>

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
    struct environment {
        environment(pkg_chk::options const& opts);

        std::shared_future<std::filesystem::path> PKG_PATH;
        std::shared_future<std::string>           MACHINE_ARCH;
        std::shared_future<std::filesystem::path> MAKECONF;
        std::shared_future<std::string>           OPSYS;
        std::shared_future<std::string>           OS_VERSION;
        std::shared_future<std::filesystem::path> PKGSRCDIR;
        std::shared_future<std::filesystem::path> PACKAGES;
        std::shared_future<std::filesystem::path> PKG_DBDIR;
        std::shared_future<std::string>           PKG_INFO;
        std::shared_future<std::string>           PKG_SUFX;
        std::shared_future<std::filesystem::path> PKGCHK_CONF;
        std::shared_future<tagset>                PKGCHK_NOTAGS;
        std::shared_future<tagset>                PKGCHK_TAGS;
        std::shared_future<std::filesystem::path> PKGCHK_UPDATE_CONF;
        std::shared_future<std::string>           SU_CMD;

        std::shared_future<tagset> included_tags;
        std::shared_future<tagset> excluded_tags;
    };
}
