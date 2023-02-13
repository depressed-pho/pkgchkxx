#pragma once

#include <set>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace pkg_chk {
    using tag = std::string;

    struct tagset: public std::set<tag> {
        using std::set<tag>::set;

        tagset(std::string const& tags)
            : tagset(static_cast<std::string_view>(tags)) {}

        /// Construct a set of tags by splitting comma-separated tags.
        tagset(std::string_view const& tags) {
            if (!tags.empty()) {
                std::string::size_type last_sep = -1;
                while (true) {
                    auto const next_sep = tags.find_first_of(',', last_sep + 1);
                    if (next_sep == std::string::npos) {
                        emplace(tags.substr(last_sep + 1));
                        break;
                    }
                    else {
                        last_sep = next_sep;
                    }
                }
            }
        }
    };

    inline std::ostream&
    operator<< (std::ostream& out, tagset const& tags) {
        bool is_first = true;
        for (auto const& t: tags) {
            if (is_first) {
                is_first = false;
            }
            else {
                out << ',';
            }
            out << t;
        }
        return out;
    }

    /// Tag pattern: ["-"] TAG *("+" TAG)
    struct tagpat {
        tagpat(std::string_view const& pattern);

        bool negative;
        std::vector<tag> tags_and;
    };

    inline std::ostream&
    operator<< (std::ostream& out, tagpat const& pat) {
        if (pat.negative) {
            out << '-';
        }

        bool is_first = true;
        for (auto const& t: pat.tags_and) {
            if (is_first) {
                is_first = false;
            }
            else {
                out << '+';
            }
            out << t;
        }
        return out;
    }
}
