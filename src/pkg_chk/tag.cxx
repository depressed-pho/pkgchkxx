#include <array>
#include <optional>

#include <pkgxx/string_algo.hxx>

#include "tag.hxx"

namespace pkg_chk {
    tagset::tagset(std::string_view const& tags) {
        for (auto const& tag: pkgxx::words(tags, ",")) {
            emplace(tag);
        }
    }

    std::ostream&
    operator<< (std::ostream& out, tagset const& in) {
        return pkgxx::join(out, in, ',');
    }

    std::istream&
    operator>> (std::istream& in, tagset& out) {
        // Let's be lazy, and read the entire input from the stream. We
        // will load everything onto the memory anyway.
        std::string tags;
        std::array<decltype(tags)::value_type, 1024> buf;
        while (in.good()) {
            in.read(buf.data(), buf.size());
            tags.append(buf.data(), static_cast<decltype(tags)::size_type>(in.gcount()));
        }

        // Then parse it.
        out.clear();
        for (auto const& tag: pkgxx::words(tags, ",")) {
            out.emplace(tag);
        }
        return in;
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
        return pkgxx::join(out, pat.tags_and, '+');
    }
}
