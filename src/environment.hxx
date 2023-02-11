#pragma once

#include <filesystem>
#include <future>
#include <string>

#include "options.hxx"

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
        virtual ~environment() {}

        std::shared_future<std::filesystem::path> PKG_PATH;
        std::shared_future<std::filesystem::path> MAKECONF;
        std::shared_future<std::filesystem::path> PKGSRCDIR;
        std::shared_future<std::filesystem::path> PACKAGES;
    };
}
