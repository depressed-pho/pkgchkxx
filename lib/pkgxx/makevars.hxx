#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <vector>

namespace pkgxx {
    /** Extract a set of variables from a given mk.conf. 'vars' is a
     * sequence of variables to extract. Returns a map from variable names
     * to their value which is possibly empty, or std::nullopt_t if the
     * mk.conf doesn't exist.
     */
    std::optional<
        std::map<std::string, std::string>>
    extract_mkconf_vars(
        std::filesystem::path const& makeconf,
        std::vector<std::string> const& vars,
        std::map<std::string, std::string> const& assignments = {});

    /** Extract a set of variables from a Makefile in an absolute path to a
     * package directory. 'vars' is a sequence of variables to
     * extract. Returns a map from variable names to their value which is
     * possibly empty, or std::nullopt if the Makefile doesn't exist.
     */
    std::optional<
        std::map<std::string, std::string>>
    extract_pkgmk_vars(
        std::filesystem::path const& pkgdir,
        std::vector<std::string> const& vars,
        std::map<std::string, std::string> const& assignments = {});

    /** A variant of extract_pkgmk_vars() that extracts a value of a single
     * variable. \c T must be a type where <tt>T(std::string&&)</tt> is
     * well-formed.
     */
    template <typename T = std::string>
    inline std::optional<T>
    extract_pkgmk_var(
        std::filesystem::path const& pkgdir,
        std::string const& var,
        std::map<std::string, std::string> const& assignments = {}) {

        // std::optional<T>::transform() is a C++23 thing. We can't use it
        // atm.
        if (auto value_of = extract_pkgmk_vars(pkgdir, {var}, assignments); value_of) {
            return T(std::move((*value_of)[var]));
        }
        else {
            return std::nullopt;
        }
    }
}
