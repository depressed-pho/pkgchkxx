#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <optional>
#include <thread>

#include <pkgxx/environment.hxx>
#include <pkgxx/pkgname.hxx>

#include "message.hxx"
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

        pkgxx::maybe_tty_osyncstream
        raw_msg() const;

        msgstream
        verbose(unsigned level = 1) const;

        msgstream
        verbose(pkgxx::ttystream_base& out, unsigned level = 1) const;

        virtual void
        verbose_var(
            std::string_view const& var,
            std::string_view const& value) const override;

        void
        verbose_var(
            std::string_view const& var,
            std::string_view const& value,
            pkgxx::ttystream_base& out) const;

        msgstream
        msg() const;

        msgstream
        msg(pkgxx::ttystream_base& out) const;

        msgstream
        warn() const;

        msgstream
        warn(pkgxx::ttystream_base& out) const;

        msgstream
        error() const;

        msgstream
        error(pkgxx::ttystream_base& out) const;

        [[noreturn]] void
        fatal(std::function<void (msgstream&)> const& f) const;

        [[noreturn]] void
        fatal(std::function<void (msgstream&)> const& f,
              pkgxx::ttystream_base& out) const;

        template <typename Rep, typename Period>
        void vsleep(
            const std::chrono::duration<Rep, Period>& duration,
            unsigned level = 2) const {

            if (opts.verbose >= level) {
                std::this_thread::sleep_for(duration);
            }
        }

        pkg_rr::options const& opts;
        std::shared_future<std::optional<pkgxx::pkgbase>> FETCH_USING;
        std::shared_future<std::string> PKG_ADMIN;
        std::shared_future<std::string> PKG_INFO;
        std::shared_future<std::string> SU_CMD;

    private:
        std::shared_ptr<pkgxx::maybe_ttystream> _cerr;
    };
}
