#include <optional>

#include <pkgxx/string_algo.hxx>

#include "tag.hxx"

namespace pkg_chk {
    tagset::tagset(std::string_view const& tags) {
        if (!tags.empty()) {
            std::optional<std::string::size_type> last_sep;
            while (true) {
                auto const next_sep =
                    last_sep ? tags.find_first_of(',', *last_sep + 1)
                             : tags.find_first_of(',');
                if (next_sep == std::string::npos) {
                    if (last_sep) {
                        emplace(tags.substr(*last_sep + 1));
                    }
                    else {
                        emplace(tags);
                    }
                    break;
                }
                else {
                    last_sep = next_sep;
                }
            }
        }
    }

    std::ostream&
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
        for (const auto& t: pkgxx::words(tags, "+")) {
            tags_and.emplace_back(t);
        }
    }

    std::ostream&
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
