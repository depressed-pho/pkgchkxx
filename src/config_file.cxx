#include <algorithm>
#include <cassert>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <type_traits>

#include "config_file.hxx"
#include "string_algo.hxx"

namespace fs = std::filesystem;

namespace pkg_chk {
    config::group_def::group_def(std::string_view const& line) {
        auto const equal = line.find('=');
        assert(equal != std::string_view::npos);

        group = trim(line.substr(0, equal));
        auto const pats =
            equal + 1 <= line.size() ? line.substr(equal + 1) : std::string_view();
        for (auto const& pat: words(pats)) {
            patterns_or.emplace_back(pat);
        }
    }

    config::pkg_def::pkg_def(std::string_view const& line) {
        bool is_first = true;
        for (auto const word: words(line)) {
            if (is_first) {
                pkgdir   = word;
                is_first = false;
            }
            else {
                patterns_or.emplace_back(word);
            }
        }
    }

    config::config(std::filesystem::path const& file) {
        std::ifstream in(file, std::ios_base::in);
        if (!in) {
            throw std::system_error(errno, std::generic_category(), "Failed to open " + file.string());
        }
        in.exceptions(std::ios_base::badbit);

        for (std::string line; std::getline(in, line); ) {
            auto const hash = line.find('#');
            if (hash != std::string::npos) {
                line.erase(hash);
            }

            auto const equal = line.find('=');
            if (equal != std::string::npos) {
                // Lines containing '=' are group definition lines.
                emplace_back(group_def(line));
            }
            else {
                auto const non_space = line.find_first_not_of(" \t");
                if (non_space != std::string::npos) {
                    // Lines containing no '=' but have anything but spaces
                    // are package definition lines.
                    emplace_back(pkg_def(line));
                }
            }
        }
    }

    std::set<std::filesystem::path>
    config::apply_tags(tagset const& included_tags, tagset const& excluded_tags) const {
        std::set<std::filesystem::path> pkgdirs;

        tagset current_tags; // included_tags - excluded_tags
        std::set_difference(
            included_tags.begin(), included_tags.end(),
            excluded_tags.begin(), excluded_tags.end(),
            std::inserter(current_tags, current_tags.begin()));

        auto const eval_and =
            [&](std::vector<tag> const& tags_and) -> bool {
                if (current_tags.count("*")) {
                    // A special case: everything matches.
                    return true;
                }
                else {
                    for (auto const& t: tags_and) {
                        if (t.size() > 0 && t[0] == '/') {
                            // Tags beginning with a slash denote file
                            // tests.
                            if (current_tags.count(t) > 0) {
                                // The user asked to ignore the actual
                                // existence and assume it exists.
                                continue;
                            }
                            else if (excluded_tags.count(t) > 0) {
                                // The user asked to ignore the actual
                                // existence and assume it doesn't exist.
                                return false;
                            }
                            else {
                                if (!fs::exists(t)) {
                                    return false;
                                }
                            }
                        }
                        else {
                            if (current_tags.count(t) == 0) {
                                return false;
                            }
                        }
                    }
                    return true;
                }
            };

        auto const eval_or =
            [&](std::vector<tagpat> const& patterns_or) -> bool {
                bool matched = false;
                for (auto const& pat: patterns_or) {
                    if (pat.negative) {
                        // -tag1+tag2 does NOT mean !(tag1 && tag2).
                        if (eval_and(pat.tags_and)) {
                            return false;
                        }
                        else {
                            matched = true;
                        }
                    }
                    else {
                        if (eval_and(pat.tags_and)) {
                            matched = true;
                        }
                    }
                }
                return matched;
            };

        for (auto const& def: _defs) {
            std::visit(
                [&](auto const& d) {
                    if constexpr (std::is_same_v<decltype(d), group_def const&>) {
                        if (eval_or(d.patterns_or)) {
                            // One of a pattern in a group definition
                            // matches. Add the group to the current set of
                            // tags.
                            current_tags.insert(d.group);
                        }
                    }
                    else {
                        if (d.patterns_or.empty() || eval_or(d.patterns_or)) {
                            // The package definition has no patterns, or
                            // one of a pattern matches. Add the pkgdir to
                            // the result.
                            pkgdirs.insert(d.pkgdir);
                        }
                    }
                },
                def);
        }

        return pkgdirs;
    }
}
