#pragma once

#include <future>
#include <pkgxx/environment.hxx>
#include <pkgxx/pkgpath.hxx>
#include <pkgxx/tty.hxx>
#include <unordered_set>

#include "options.hxx"

namespace lint {
    /** Values from the environment such as various Makefiles. Most of such
     * values are very expensive to retrieve so they are lazily
     * evaluated.
     *
     * Objects of this class MUST NOT be shared by threads. Each thread
     * must have its own copy.
     */
    struct environment: public pkgxx::environment {
        environment(lint::options const& opts);

        pkgxx::maybe_tty_osyncstream
        out() const;

        pkgxx::maybe_tty_osyncstream
        msg() const;

        pkgxx::maybe_tty_osyncstream
        warn() const;

        pkgxx::maybe_tty_osyncstream
        verbose() const;

        virtual void
        verbose_var(
            std::string_view const& var,
            std::string_view const& value) const override;

        lint::options const& opts;
        std::shared_future<std::filesystem::path> PKGSRCDIR; // e.g. /usr/pkgsrc
        std::shared_future<
            std::unordered_set<pkgxx::category>
            > official_categories;
        std::shared_future<
            std::unordered_set<pkgxx::category>
            > all_categories;
        std::shared_future<
            std::unordered_set<pkgxx::pkgpath>
            > all_pkgpaths;

    private:
        std::shared_ptr<pkgxx::maybe_ttystream> _cout;
        std::shared_ptr<pkgxx::maybe_ttystream> _cerr;
    };
}
