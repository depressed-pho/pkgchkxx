#include <map>
#include <string>
#include <vector>

#include "config.h"
#include "harness.hxx"
#include "makevars.hxx"

namespace fs = std::filesystem;

namespace pkgxx {
    std::optional<
        std::map<std::string, std::string>>
    extract_mkconf_vars(
        std::filesystem::path const& makeconf,
        std::vector<std::string> const& vars,
        std::map<std::string, std::string> const& assignments) {

        if (!fs::exists(makeconf)) {
            return std::nullopt;
        }

        std::map<std::string, std::string> value_of;
        if (vars.empty()) {
            // No variables to extract.
            return value_of;
        }
        else {
            std::vector<std::string> argv = {
                CFG_BMAKE, "-f", "-", "-f", makeconf, "x"
            };
            for (auto const& pair: assignments) {
                argv.push_back(pair.first + '=' + pair.second);
            }
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
            }
            make.cout().close();

            return value_of;
        }
    }

    std::optional<
        std::map<std::string, std::string>>
    extract_pkgmk_vars(
        std::filesystem::path const& pkgdir,
        std::vector<std::string> const& vars,
        std::map<std::string, std::string> const& assignments) {

        if (!fs::exists(pkgdir / "Makefile")) {
            return std::nullopt;
        }

        std::map<std::string, std::string> value_of;
        if (vars.empty()) {
            // No variables to extract.
            return value_of;
        }
        else {
            std::vector<std::string> argv = {
                CFG_BMAKE, "-f", "-", "-f", "Makefile", "x"
            };
            for (auto const& pair: assignments) {
                argv.push_back(pair.first + '=' + pair.second);
            }
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
            }
            make.cout().close();

            return value_of;
        }
    }
}
