#include <cstdlib>
#include <iostream>
#include <optional>
#include <pkgxx/shell_args.hxx>
#include <thread>
#include <vector>

#include "options.hxx"

namespace fs = std::filesystem;

namespace pkg_chk {
    options::options(int argc, char* const argv[])
        : _progname{argv[0]}
        , _op{"Allowed options"} {

        auto a = _op.add<popl::Switch>("a", "", "Add all missing packages", &add_missing);
        _op.add<popl::Switch>("B", "", "Force exact pkg match - check \"Build version\" & even downgrade", &check_build_version);
        _op.add<popl::Switch>("b", "", "Use binary packages", &use_binary_pkgs);
        auto C = _op.add<popl::Value<fs::path>>("C", "", "Use a pkgchk.conf file");
        auto c = _op.add<popl::Switch, popl::Attribute::hidden>("c", "", "");
        auto D = _op.add<popl::Value<tagset>>("D", "", "Comma separated list of additional pkgchk.conf tags to set");
        _op.add<popl::Switch>("d", "", "Do not clean the pkg build directories", &no_clean);
        _op.add<popl::Switch>("f", "", "Perform a 'make fetch' for all required packages", &fetch);
        auto g = _op.add<popl::Switch>("g", "", "Generate an initial pkgchk.conf file");
        auto h = _op.add<popl::Switch>("h", "", "Print this help");
        auto i = _op.add<popl::Switch>("i", "", "");
        _op.add<popl::Value<pkgxx::positive<unsigned>>>(
            "j", "", "Parallelize certain operations with a given concurrency",
            std::max(1u, std::thread::hardware_concurrency()),
            &concurrency);
        _op.add<popl::Switch>("k", "", "Continue with further packages if errors are encountered", &continue_on_errors);
        auto L = _op.add<popl::Value<fs::path>>("L", "", "Redirect output from commands run into file");
        auto l = _op.add<popl::Switch>("l", "", "List binary packages including dependencies");
        auto N = _op.add<popl::Switch>("N", "", "List installed packages for which a newer version is in TODO");
        _op.add<popl::Switch>("n", "", "Display actions that would be taken, but do not perform them", &dry_run);
        auto P = _op.add<popl::Value<fs::path>>("P", "", "Set PACKAGES dir (overrides any other setting)");
        _op.add<popl::Switch>("p", "", "Display the list of pkgpaths that match the current tags", &print_pkgpaths_to_check);
        _op.add<popl::Switch>("q", "", "Do not display actions or take any action; only list packages", &list_ver_diffs);
        auto r = _op.add<popl::Switch>("r", "", "Recursively remove mismatches (use with care)", &delete_mismatched);
        _op.add<popl::Switch>("s", "", "Use source for building packages", &build_from_source);
        auto U = _op.add<popl::Value<tagset>>("U", "", "Comma separated list of pkgchk.conf tags to unset ('*' for all)");
        auto u = _op.add<popl::Switch>("u", "", "Update all mismatched packages", &update);
        _op.add<popl::Switch>("v", "", "Be verbose", &verbose);
        _op.parse(argc, argv);

        std::optional<pkg_chk::mode> mode_;
        if (a->is_set()) {
            mode_ = mode::ADD_DELETE_UPDATE;
        }
        if (C->is_set()) {
            pkgchk_conf_path = C->value();
        }
        if (c->is_set()) {
            std::cerr << _progname.string() << ": option -c is deprecated. Use -a -q" << std::endl;
            mode_          = mode::ADD_DELETE_UPDATE;
            add_missing    = true;
            list_ver_diffs = true;
        }
        if (D->is_set()) {
            add_tags = D->value();
        }
        if (g->is_set()) {
            mode_ = mode::GENERATE_PKGCHK_CONF;
        }
        if (h->is_set()) {
            mode_ = mode::HELP;
        }
        if (i->is_set()) {
            std::cerr << argv[0] << ": option -i is deprecated. Use -u -q" << std::endl;
        }
        if (L->is_set()) {
            auto const path = L->value();

            logfile.open(path, std::ios_base::app);
            if (!logfile) {
                using namespace std::literals;
                throw std::system_error(errno, std::generic_category(), "Failed to open: "s + path.string());
            }
            logfile.exceptions(std::ios_base::badbit);
        }
        if (l->is_set()) {
            mode_ = mode::LIST_BIN_PKGS;
        }
        if (N->is_set()) {
            mode_ = mode::LOOKUP_TODO;
        }
        if (P->is_set()) {
            bin_pkg_path = P->value();
        }
        if (r->is_set()) {
            mode_ = mode::ADD_DELETE_UPDATE;
        }
        if (U->is_set()) {
            remove_tags = U->value();
        }
        if (u->is_set()) {
            mode_ = mode::ADD_DELETE_UPDATE;
        }

        if (mode_) {
            mode = *mode_;
        }
        else {
            throw std::runtime_error{"must specify at least one of -a, -g, -l, -r, -u, or -N"};
        }

        if (fetch && !build_from_source) {
            throw std::runtime_error{
                "-f is an option to pre-fetch source distributions to build packages,"
                " which does not make sense if one doesn't intend to build them"};
        }

        if (!use_binary_pkgs && !build_from_source) {
            use_binary_pkgs   = true;
            build_from_source = true;
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
        std::cerr
            << "Usage: " << progbase << " [opts]" << std::endl
            << std::endl
            << _op.help()
            << std::endl
            << progbase << " verifies installed packages against pkgsrc." << std::endl
            << "The most common usage is '" << progbase << " -u -q' to check all installed packages or" << std::endl
            << '\'' << progbase << " -u' to update all out of date packages." << std::endl
            << "For more advanced usage, including defining a set of desired packages based" << std::endl
            << "on hostname and type, see " << progbase << "(8)." << std::endl
            << std::endl
            << "If neither -b nor -s is given, both are assumed with -b preferred." << std::endl;
    }
}
