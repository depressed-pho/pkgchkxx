#include <map>
#include <string>
#include <vector>

#include "harness.hxx"
#include "makevars.hxx"
#include "message.hxx"

namespace pkg_chk {
    std::map<std::string, std::string>
    extract_mkconf_vars(
        pkg_chk::options const& opts,
        std::filesystem::path const& makeconf,
        std::vector<std::string> const& vars) {

        std::map<std::string, std::string> value_of;

        if (vars.empty()) {
            // No variables to extract.
            return value_of;
        }
        else {
            std::vector<std::string> const argv = {
                CFG_BMAKE, "-f", "-", "-f", makeconf, "x"
            };
            harness make(CFG_BMAKE, argv);

            make.cin()
                << "BSD_PKG_MK=1" << std::endl
                << ".PHONY: x"    << std::endl
                << "x:"           << std::endl;
            for (auto const& var: vars) {
                make.cin()
                    << "\t@printf '%s\\0' \"${" << var << "}\"" << std::endl;
            }
            make.cin().close();

            for (std::vector<std::string>::size_type i = 0; i < vars.size(); i++) {
                std::string value;
                std::getline(make.cout(), value, '\0');

                value_of[vars[i]] = value;
                verbose_var(opts, vars[i], value);
            }
            make.cout().close();

            return value_of;
        }
    }

    std::map<std::string, std::string>
    extract_pkgmk_vars(
        pkg_chk::options const& opts,
        std::filesystem::path const& pkgdir,
        std::vector<std::string> const& vars) {

        std::map<std::string, std::string> value_of;

        if (vars.empty()) {
            // No variables to extract.
            return value_of;
        }
        else {
            std::vector<std::string> const argv = {
                CFG_BMAKE, "-f", "-", "-f", "Makefile", "x"
            };
            harness make(CFG_BMAKE, argv, pkgdir);

            make.cin()
                << ".PHONY: x" << std::endl
                << "x:"        << std::endl;
            for (auto const& var: vars) {
                make.cin()
                    << "\t@printf '%s\\0' \"${" << var << "}\"" << std::endl;
            }
            make.cin().close();

            for (std::vector<std::string>::size_type i = 0; i < vars.size(); i++) {
                std::string value;
                std::getline(make.cout(), value, '\0');

                value_of[vars[i]] = value;
                verbose_var(opts, vars[i], value);
            }
            make.cout().close();

            return value_of;
        }
    }
}
