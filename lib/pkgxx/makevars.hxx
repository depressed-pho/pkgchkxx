#pragma once

#include <filesystem>
#include <optional>
#include <pkgxx/config.h>
#include <pkgxx/harness.hxx>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pkgxx {
    /** Extract a set of variables from a given mk.conf. 'vars' is a
     * sequence of variables to extract. Returns a map from variable names
     * to their value which is possibly empty, or std::nullopt_t if the
     * mk.conf doesn't exist. 'assignments' is a set of variables to assign
     * while querying the mk.conf.
     */
    template <template <typename, typename> typename Map = std::unordered_map,
              typename Vars        = std::unordered_set<std::string_view>,
              typename Assignments = std::unordered_map<std::string_view, std::string_view>>
    std::optional<
        Map<std::string, std::string>>
    extract_mkconf_vars(
        std::filesystem::path const& makeconf,
        Vars                  const& vars,
        Assignments           const& assignments = {});

    /** A variant of extract_mkconf_vars() that extracts a value of a
     * single variable.
     */
    template <typename Var         = std::string_view,
              typename Assignments = std::unordered_map<std::string_view, std::string_view>>
    std::optional<
        std::optional<std::string>>
    extract_mkconf_var(
        std::filesystem::path const& makeconf,
        Var                   const& var,
        Assignments           const& assignments = {});

    /** Extract a set of variables from a Makefile in an absolute path to a
     * package directory. 'vars' is a sequence of variables to
     * extract. Returns a map from variable names to their value which is
     * possibly empty, or std::nullopt if the Makefile doesn't exist.
     */
    template <template <typename, typename> typename Map = std::unordered_map,
              typename Vars        = std::unordered_set<std::string_view>,
              typename Assignments = std::unordered_map<std::string_view, std::string_view>>
    std::optional<
        Map<std::string, std::string>>
    extract_pkgmk_vars(
        std::filesystem::path const& pkgdir,
        Vars                  const& vars,
        Assignments           const& assignments = {});

    /** A variant of extract_pkgmk_vars() that extracts a value of a single
     * variable.
     */
    template <typename Var         = std::string_view,
              typename Assignments = std::unordered_map<std::string_view, std::string_view>>
    inline std::optional<std::string>
    extract_pkgmk_var(
        std::filesystem::path const& pkgdir,
        Var                   const& var,
        Assignments           const& assignments = {});

    // Implementation

    template <template <typename, typename> typename Map,
              typename Vars,
              typename Assignments>
    std::optional<
        Map<std::string, std::string>>
    extract_mkconf_vars(
        std::filesystem::path const& makeconf,
        Vars                  const& vars,
        Assignments           const& assignments) {

        namespace fs = std::filesystem;

        if (!fs::exists(makeconf)) {
            return std::nullopt;
        }

        Map<std::string, std::string> value_of;
        if (vars.empty()) {
            // No variables to extract.
            return value_of;
        }
        else {
            std::vector<std::string> argv = {
                CFG_BMAKE, "-f", "-", "-f", makeconf, "x"
            };
            for (auto const& [var, value]: assignments) {
                std::string tmp{var};
                tmp += '=';
                tmp += value;
                argv.push_back(std::move(tmp));
            }
            harness make(CFG_BMAKE, argv);

            make.cin()
                << "BSD_PKG_MK=1" << std::endl
                << ".PHONY: x"    << std::endl
                << "x:"           << std::endl;
            std::vector<std::string_view> ordered_vars;
            ordered_vars.reserve(vars.size());
            for (auto const& var: vars) {
                ordered_vars.emplace_back(var);
                make.cin()
                    << "\t@printf '%s\\0' ${" << var << ":U__pkgxx_undefined__:Q}" << std::endl;
            }
            make.cin().close();

            for (decltype(ordered_vars)::size_type i = 0; i < ordered_vars.size(); i++) {
                std::string value;
                std::getline(make.cout(), value, '\0');

                if (value != "__pkgxx_undefined__") {
                    value_of.emplace(ordered_vars[i], std::move(value));
                }
            }
            make.cout().close();

            return value_of;
        }
    }

    template <typename Var,
              typename Assignments>
    std::optional<
        std::optional<std::string>>
    extract_mkconf_var(
        std::filesystem::path const& makeconf,
        Var                   const& var,
        Assignments           const& assignments) {

        if (auto value_of = extract_mkconf_vars(makeconf, {var}, assignments); value_of) {
            if (auto it = value_of->find(var); it != value_of->end()) {
                return std::make_optional(std::move(it->second));
            }
            else {
                return {};
            }
        }
        else {
            return {};
        }
    }

    template <template <typename, typename> typename Map,
              typename Vars,
              typename Assignments>
    std::optional<
        Map<std::string, std::string>>
    extract_pkgmk_vars(
        std::filesystem::path const& pkgdir,
        Vars                  const& vars,
        Assignments           const& assignments) {

        namespace fs = std::filesystem;

        if (!fs::exists(pkgdir / "Makefile")) {
            return std::nullopt;
        }

        Map<std::string, std::string> value_of;
        if (vars.empty()) {
            // No variables to extract.
            return value_of;
        }
        else {
            std::vector<std::string> argv = {
                CFG_BMAKE, "-f", "-", "-f", "Makefile", "x"
            };
            for (auto const& [var, value]: assignments) {
                std::string tmp{var};
                tmp += '=';
                tmp += value;
                argv.push_back(std::move(tmp));
            }
            harness make(CFG_BMAKE, argv, "cwd"_na = std::optional(pkgdir));

            make.cin()
                << ".PHONY: x" << std::endl
                << "x:"        << std::endl;
            std::vector<std::string_view> ordered_vars;
            ordered_vars.reserve(vars.size());
            for (auto const& var: vars) {
                ordered_vars.emplace_back(var);
                make.cin()
                    << "\t@printf '%s\\0' ${" << var << ":U__pkgxx_undefined__:Q}" << std::endl;
            }
            make.cin().close();

            for (decltype(ordered_vars)::size_type i = 0; i < ordered_vars.size(); i++) {
                std::string value;
                std::getline(make.cout(), value, '\0');

                if (value != "__pkgxx_undefined__") {
                    value_of.emplace(ordered_vars[i], std::move(value));
                }
            }
            make.cout().close();

            return value_of;
        }
    }

    template <typename Var,
              typename Assignments>
    inline std::optional<std::string>
    extract_pkgmk_var(
        std::filesystem::path const& pkgdir,
        Var                   const& var,
        Assignments           const& assignments) {

        if (auto value_of = extract_pkgmk_vars(pkgdir, {var}, assignments); value_of) {
            if (auto it = value_of->find(var); it != value_of->end()) {
                return std::make_optional(std::move(it->second));
            }
            else {
                return {};
            }
        }
        else {
            return {};
        }
    }
}
