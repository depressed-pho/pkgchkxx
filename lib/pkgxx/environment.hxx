#pragma once

#include <filesystem>
#include <functional>
#include <future>
#include <string_view>

namespace pkgxx {
    /** C++ wrapper for POSIX getenv(3) */
    std::string
    cgetenv(std::string const& name);

    /** Values from the environment such as various Makefiles. Most of such
     * values are very expensive to retrieve so they are lazily
     * evaluated.
     *
     * Objects of this class MUST NOT be shared by threads. Each thread
     * must have its own copy.
     */
    struct environment {
        /** Obtain values from the environment. */
        environment(
            std::function<
                void (std::string_view const&, std::string_view const&)
                > const& var_logger = [](auto, auto) {});

        std::shared_future<std::filesystem::path> MAKECONF;  ///< Path to mk.conf
        std::shared_future<std::filesystem::path> PKG_PATH;  ///< For pkg_add(1)
        std::shared_future<std::filesystem::path> PKGSRCDIR; ///< Base of pkgsrc tree

    private:
        std::function<
            void (std::string_view const&, std::string_view const&)
            > _var_logger;
    };
}
