#include <cstdlib>
#include <iostream>
#include <pkgxx/shell_args.hxx>
#include <thread>

#include "options.hxx"

namespace fs = std::filesystem;

namespace lint {
    options::options(int argc, char* const argv[])
        : _progname{argv[0]}
        , _op{"Allowed options"} {

        // General
        _op.add<popl::Switch>("h", "", "Print this help (see lintpkgxx(1) for more information", &help);
        _op.add<popl::Value<pkgxx::positive<unsigned>>>(
            "j", "", "Parallelize certain operations with a given concurrency",
            std::max(1u, std::thread::hardware_concurrency()),
            &concurrency);
        _op.add<popl::Switch>("v", "", "Be verbose", &verbose);

        // Makefile
        _op.add<popl::Switch>("B", "", "List packages marked as 'BROKEN'", &list_broken);
        auto D = _op.add<popl::Switch, popl::Attribute::hidden>("D", "", "Debug makefile and glob parsing");

        // Modifiers
        auto P = _op.add<popl::Value<fs::path>>("P", "", "Set PKGSRCDIR");
        _op.add<popl::Switch>("L", "", "List each Makefile when scanned", &list_makefiles);

        _op.parse(argc, argv);

        if (P->is_set()) {
            PKGSRCDIR = P->value();
        }

        if (auto const& unknowns = _op.unknown_options(); !unknowns.empty()) {
            throw std::runtime_error{
                "unknown options: " + pkgxx::shell_args(unknowns).string()};
        }

        // -D is unsupported and ignored, but it takes any number of
        // arguments. Allow non-options in this case.
        if (auto const& non_opts = _op.non_option_args(); !non_opts.empty() && !D->is_set()) {
            throw std::runtime_error{
                "unknown arguments: " + pkgxx::shell_args(non_opts).string()};
        }
    }

    void
    options::usage() const {
        auto const& progbase = _progname.filename().string();
        std::cerr
            << "Usage: " << progbase << " [options] [makefiles]" << std::endl
            << std::endl
            << _op.help();
    }
}
