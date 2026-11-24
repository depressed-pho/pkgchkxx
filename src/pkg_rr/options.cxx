#include <cstdlib>
#include <pkgxx/shell_args.hxx>
#include <pkgxx/string_algo.hxx>
#include <sstream>
#include <string_view>
#include <thread>

#include "options.hxx"

namespace fs = std::filesystem;

namespace {
    struct var_def: public std::pair<std::string, std::string> {
        using std::pair<std::string, std::string>::pair;
    };

    std::ostream&
    operator<< (std::ostream& out, var_def const& in) {
        return out << in.first << '=' << in.second;
    }

    std::istringstream&
    operator>> (std::istringstream& in, var_def& out) {
        auto const& str   = in.str();
        auto const  equal = str.find('=');

        if (equal != std::string_view::npos) {
            out.first  = str.substr(0, equal);
            out.second = str.substr(equal + 1);
        }
        else {
            in.setstate(std::ios_base::failbit);
        }
        return in;
    }

    struct excludes: public std::unordered_set<pkgxx::pkgbase> {
        using std::unordered_set<pkgxx::pkgbase>::unordered_set;
    };

    std::ostream&
    operator<< (std::ostream& out, excludes const& in) {
        return pkgxx::join(out, in, ',');
    }

    std::istringstream&
    operator>> (std::istringstream& in, excludes& out) {
        for (auto const& base: pkgxx::words(in.str(), ",")) {
            out.emplace(base);
        }
        return in;
    }
}

namespace pkg_rr {
    options::options(int argc, char* const argv[])
        : _progname{argv[0]}
        , _op{"Allowed options"} {

        _op.add<popl::Switch>("h", "", "Print this help", &help);
        _op.add<popl::Switch>("B", "", "Force exact pkg match - check \"Build version\"", &check_build_version);
        _op.add<popl::Switch>("F", "", "Fetch sources (including depends) only, don't build", &just_fetch);
        _op.add<popl::Value<pkgxx::positive<unsigned>>>(
            "j", "", "Parallelize certain operations with a given concurrency",
            std::max(1u, std::thread::hardware_concurrency()),
            &concurrency);
        _op.add<popl::Switch>("k", "", "Keep running, even on error", &continue_on_errors);
        _op.add<popl::Switch>("n", "", "Display actions to be taken but don't actuall run them", &dry_run);
        _op.add<popl::Switch>("r", "", "Just replace, don't create binary packages", &just_replace);
        _op.add<popl::Switch>("s", "", "Replace even if the ABIs are still compatible (\"strict\")", &strict);
        _op.add<popl::Switch>("u", "", "Check for mismatched packages and mark them as so", &check_for_updates);
        auto v = _op.add<popl::Switch>("v", "", "Be verbose");
        auto D = _op.add<popl::Value<var_def>>("D", "", "Pass given variables and values to make(1)");
        auto L = _op.add<popl::Value<fs::path>>("L", "", "Log to path ({PATH}/{pkgdir}/{pkg})");
        auto X = _op.add<popl::Value<excludes>>("X", "", "Packages to exclude from being rebuilt");
        auto x = _op.add<popl::Value<excludes>>("x", "", "Packages to exclude from mismatch check");
        _op.parse(argc, argv);

        verbose = static_cast<unsigned>(v->count());

        make_vars["IN_PKG_ROLLING_REPLACE"] = "1";
        for (std::size_t i = 0; i < D->count(); i++) {
            make_vars.insert(D->value(i));
        }

        if (L->is_set()) {
            log_dir = L->value();
        }

        if (X->is_set()) {
            no_rebuild = X->value();
        }

        if (x->is_set()) {
            no_check = x->value();
        }

        if (auto const& unknowns = _op.unknown_options(); !unknowns.empty()) {
            throw std::runtime_error{
                "unknown options: " + pkgxx::shell_args(unknowns).string()};
        }

        if (auto const& non_opts = _op.non_option_args(); !non_opts.empty()) {
            throw std::runtime_error{
                "unknown arguments: " + pkgxx::shell_args(non_opts).string()};
        }
    }

    void
    options::usage() {
        auto const& progbase = _progname.filename().string();
        std::cout
            << "Usage: " << progbase << " [opts]" << std::endl
            << std::endl
            << _op.help()
            << std::endl
            << progbase << " does `make replace' on one package at a time," << std::endl
            << "tsorting the packages being replaced according to their" << std::endl
            << "interdependencies, which avoids most duplicate rebuilds." << std::endl
            << std::endl
            << progbase << " can be used in one of two ways:" << std::endl
            << std::endl
            << "    - `make replace' is unsafe in that, if the replaced package's ABI" << std::endl
            << "      changes, its dependent packages may break.  If this happens, run" << std::endl
            << "      `" << progbase << "' (no arguments) to rebuild them against the" << std::endl
            << "      new version." << std::endl
            << std::endl
            << "    - `pkg_chk -u' will delete all your mismatched packages (where the" << std::endl
            << "      package version does not match the pkgsrc version), then reinstall" << std::endl
            << "      them one at a time, leaving you without those packages in the" << std::endl
            << "      meantime.  `" << progbase << " -u' will instead upgrade them in" << std::endl
            << "      place, allowing you to keep using your system in the meantime" << std::endl
            << "      (maybe...if you're lucky...because " << progbase << " replaces" << std::endl
            << "      the \"deepest\" dependency first, things could still break if that" << std::endl
            << "      happens to be a fundamental library whose ABI has changed)." << std::endl;
    }
}
