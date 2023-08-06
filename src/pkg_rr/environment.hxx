#pragma once

#include <future>
#include <optional>

#include <pkgxx/environment.hxx>
#include <pkgxx/pkgname.hxx>

#include "options.hxx"

namespace pkg_rr {
    /** Values from the environment such as various Makefiles. Most of such
     * values are very expensive to retrieve so they are lazily
     * evaluated.
     *
     * Objects of this class MUST NOT be shared by threads. Each thread
     * must have its own copy.
     */
    struct environment: public pkgxx::environment {
        environment(pkg_rr::options const& opts);

        std::shared_future<std::optional<pkgxx::pkgbase>> FETCH_USING;
        std::shared_future<std::string> PKG_ADMIN;
        std::shared_future<std::string> PKG_INFO;
        std::shared_future<std::string> SU_CMD;
    };
}
