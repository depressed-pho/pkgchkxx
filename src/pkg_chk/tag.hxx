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

        /** Construct a set of tags by splitting comma-separated tag
         * strings.
         */
        tagset(std::string_view const& tags);

        friend std::ostream&
        operator<< (std::ostream& out, tagset const& tags);
    };

    /// Tag pattern: ["-"] TAG *("+" TAG)
    struct tagpat {
        tagpat(std::string_view const& pattern);

        friend std::ostream&
        operator<< (std::ostream& out, tagpat const& pat);

        bool negative;
        std::vector<tag> tags_and;
    };
}
