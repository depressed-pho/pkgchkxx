#include "string_algo.hxx"
#include "tag.hxx"

namespace pkg_chk {
    tagpat::tagpat(std::string_view const& pattern) {
        std::string_view tags;
        if (pattern.size() > 0 && pattern[0] == '-') {
            negative = true;
            tags     = pattern.substr(1);
        }
        else {
            negative = false;
            tags     = pattern;
        }
        for (const auto& t: words(tags, "+")) {
            tags_and.emplace_back(t);
        }
    }
}
