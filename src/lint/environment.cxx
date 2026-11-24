#include <fstream>
#include <pkgxx/makevars.hxx>
#include <pkgxx/mutex_guard.hxx>
#include <pkgxx/nursery.hxx>
#include <pkgxx/string_algo.hxx>
#include <unistd.h>

#include "environment.hxx"

namespace fs  = std::filesystem;
namespace tty = pkgxx::tty;

namespace lint {
    environment::environment(lint::options const& opts)
        : opts(opts)
        , _cout(std::make_shared<pkgxx::maybe_ttystream>(STDOUT_FILENO))
        , _cerr(std::make_shared<pkgxx::maybe_ttystream>(STDERR_FILENO)) {

        verbose() << "Using up to " << opts.concurrency << " cores." << std::endl;

        if (opts.PKGSRCDIR) {
            // Async call is not actually needed in this case.
            PKGSRCDIR = std::async(
                std::launch::deferred,
                [&]() {
                    return *opts.PKGSRCDIR;
                }).share();
        }
        else {
            // Reuse the value from the base class.
            PKGSRCDIR = pkgxx::environment::PKGSRCDIR;
        }

        official_categories = std::async(
            std::launch::deferred,
            [this]() {
                auto const subdirs = pkgxx::extract_pkgmk_var(PKGSRCDIR.get(), "SUBDIR");
                if (subdirs.has_value()) {
                    std::unordered_set<pkgxx::category> cats;
                    for (auto const& cat : pkgxx::words(*subdirs)) {
                        fs::path const abs_cat = PKGSRCDIR.get() / cat;
                        if (fs::exists(abs_cat)) {
                            if (fs::is_directory(abs_cat)) {
                                cats.emplace(cat);
                            }
                            else {
                                warn() << "Category " << cat << " is registered in " << (PKGSRCDIR.get() / "Makefile")
                                       << " but " << (PKGSRCDIR.get() / cat) << " is not a directory." << std::endl;
                            }
                        }
                        else {
                            warn() << "Category " << cat << " is registered in " << (PKGSRCDIR.get() / "Makefile")
                                   << " but " << (PKGSRCDIR.get() / cat) << " does not exist." << std::endl;
                        }
                    }
                    {
                        auto msg = verbose();
                        msg << "Official categories:";
                        for (auto const& cat : cats) {
                            msg << ' ' << cat;
                        }
                        msg << std::endl;
                    }
                    return cats;
                }
                else {
                    warn() << "Failed to retrieve the list of official categories :"
                           << "SUBDIR not defined in " << (PKGSRCDIR.get() / "Makefile") << '.' << std::endl;
                    return std::unordered_set<pkgxx::category> {};
                }
            }).share();

        // The official list of categories can be retrieved from
        // ${PKGSRCDIR}/Makefile. However, there are unofficial or
        // semi-official ones such as wip, and users can also have their
        // own categories there. So it would be best to find categories
        // heuristically, and don't even warn about unofficial ones.
        all_categories = std::async(
            std::launch::deferred,
            [&, this]() {
                pkgxx::guarded<std::unordered_set<pkgxx::category>> cats;
                {
                    pkgxx::nursery n{opts.concurrency};
                    for (auto const& ent : fs::directory_iterator(PKGSRCDIR.get())) {
                        // If it's a directory and has a Makefile, it can
                        // be a category. See if the Makefile includes
                        // category.mk. This is a slow operation so we need
                        // to parallelise it.
                        n.start_soon(
                            [&, ent]() {
                                if (!ent.is_directory())
                                    return;

                                auto const mk = ent.path() / "Makefile";
                                if (!fs::is_regular_file(mk))
                                    return;

                                std::fstream in{mk, std::ios_base::in};
                                if (!in)
                                    throw std::system_error{
                                        errno, std::generic_category(), "Failed to open " + mk.string()};

                                for (std::string line; std::getline(in, line); ) {
                                    if (line.find("../mk/misc/category.mk") != std::string::npos) {
                                        cats.lock()->insert(ent.path().filename().string());
                                    }
                                }
                            });
                    }
                }
                // Always dereference official_categories here, so that it
                // is validated regardless of verbosity.
                {
                    auto const& official_cats = official_categories.get();
                    auto msg   = verbose();
                    bool found = false;
                    msg << "Unofficial categories:";
                    for (auto const& cat : *cats.lock()) {
                        if (!official_cats.count(cat)) {
                            found = true;
                            msg << ' ' << cat;
                        }
                    }
                    if (!found)
                        msg << " (none)";
                    msg << std::endl;
                }
                return cats.take();
            }).share();

        all_pkgpaths = std::async(
            std::launch::deferred,
            [this, opts]() {
                // This likely causes a lock contention. We should ideally
                // be using a lock-free unordered set. But is that even
                // possible...?
                pkgxx::guarded<
                    std::unordered_set<pkgxx::pkgpath>
                    > pkgpaths;
                {
                    // FIXME: Verify that each PKGDIR is registered in the category Makefile
                    pkgxx::nursery n{opts.concurrency};
                    for (auto const& cat : all_categories.get()) {
                        auto const abs_cat = PKGSRCDIR.get() / cat;
                        for (auto const& ent : fs::directory_iterator(abs_cat)) {
                            // No need to copy 'cat'. It outlives this
                            // closure.
                            n.start_soon(
                                [&pkgpaths, &cat, ent]() {
                                    // If it's a directory and has a
                                    // Makefile, it can be a PKGDIR. See if
                                    // the Makefile includes bsd.pkg.mk.
                                    if (!ent.is_directory())
                                        return;

                                    auto const mk = ent.path() / "Makefile";
                                    if (!fs::is_regular_file(mk))
                                        return;

                                    std::fstream in{mk, std::ios_base::in};
                                    if (!in)
                                        throw std::system_error{
                                            errno, std::generic_category(), "Failed to open " + mk.string()};

                                    for (std::string line; std::getline(in, line); ) {
                                        if (line.find("../mk/bsd.pkg.mk") != std::string::npos) {
                                            pkgpaths.lock()->emplace(
                                                (cat / ent.path().filename()).string());
                                        }
                                    }
                                });
                        }
                    }
                }
                verbose() << pkgpaths.lock()->size()
                          << " packages found in "
                          << PKGSRCDIR.get().string()
                          << std::endl;
                return pkgpaths.take();
            }).share();
    }

    pkgxx::maybe_tty_osyncstream
    environment::out() const {
        return pkgxx::maybe_tty_osyncstream(*_cout);
    }

    pkgxx::maybe_tty_osyncstream
    environment::msg() const {
        auto out = pkgxx::maybe_tty_osyncstream(*_cerr);
        out.push_style(
            tty::colour(tty::green),
            pkgxx::ttystreambuf_base::how::no_combine);
        return out;
    }

    pkgxx::maybe_tty_osyncstream
    environment::warn() const {
        auto out = pkgxx::maybe_tty_osyncstream(*_cerr);
        out.push_style(
            tty::bold + tty::colour(tty::yellow),
            pkgxx::ttystreambuf_base::how::no_combine);
        out << "WARNING: ";
        return out;
    }

    pkgxx::maybe_tty_osyncstream
    environment::verbose() const {
        if (opts.verbose) {
            auto out = pkgxx::maybe_tty_osyncstream {*_cerr};
            out.push_style(
                tty::dull_colour(tty::blue),
                pkgxx::ttystreambuf_base::how::no_combine);
            return out;
        }
        else {
            return {};
        }
    }

    void
    environment::verbose_var(
        std::string_view const& var,
        std::string_view const& value) const {

        verbose()
            << "Variable: " << var << " = " << (value.empty() ? "(empty)" : value) << std::endl;
    }
}
