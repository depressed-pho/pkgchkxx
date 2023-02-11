#pragma once

#include <set>
#include <string>

namespace pkg_chk {
    struct tagset: public std::set<std::string> {
        using std::set<std::string>::set;

        /// Construct a set of tags by splitting comma-separated tags.
        tagset(std::string const& tags) {
            if (!tags.empty()) {
                std::string::size_type last_sep = -1;
                while (true) {
                    auto const next_sep = tags.find_first_of(',', last_sep + 1);
                    if (next_sep == std::string::npos) {
                        insert(tags.substr(last_sep + 1));
                        break;
                    }
                    else {
                        last_sep = next_sep;
                    }
                }
            }
        }
    };
}
