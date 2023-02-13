#pragma once

#include <deque>
#include <filesystem>
#include <ostream>
#include <utility>
#include <variant>
#include <vector>

#include "tag.hxx"

namespace pkg_chk {
    struct config {
        /// Group definition line: TAG "=" *PATTERN
        struct group_def {
            group_def(std::string_view const& line);

            tag group;
            std::vector<tagpat> patterns_or;
        };

        /// Package definition line: PKGDIR *PATTERN
        struct pkg_def {
            template <typename Path, typename Patterns>
            pkg_def(Path&& pkgdir_, Patterns&& patterns_or_)
                : pkgdir(std::forward<Path>(pkgdir_))
                , patterns_or(std::forward<Patterns>(patterns_or_)) {}

            pkg_def(std::string_view const& line);

            std::filesystem::path pkgdir;
            std::vector<tagpat> patterns_or;
        };

        using definition = std::variant<
            group_def,
            pkg_def
            >;
        using value_type     = definition;
        using iterator       = std::deque<definition>::iterator;
        using const_iterator = std::deque<definition>::const_iterator;

        config() {}
        config(std::filesystem::path const& file);

        /** Obtain a set of pkgdirs in the config file, filtered by
         * applying tags. */
        std::set<std::filesystem::path>
        apply_tags(tagset const& included_tags, tagset const& excluded_tags) const;

        template <typename... Args>
        definition&
        emplace_back(Args&&... args) {
            return _defs.emplace_back(std::forward<Args...>(args...));
        }

        iterator
        begin() {
            return _defs.begin();
        }

        const_iterator
        begin() const {
            return _defs.begin();
        }

        iterator
        end() {
            return _defs.end();
        }

        const_iterator
        end() const {
            return _defs.end();
        }

    private:
        std::deque<definition> _defs;
    };

    inline std::ostream&
    operator<< (std::ostream& out, config::group_def const& def) {
        out << def.group << " =";
        for (auto const& pat: def.patterns_or) {
            out << ' ' << pat;
        }
        return out;
    }

    inline std::ostream&
    operator<< (std::ostream& out, config::pkg_def const& def) {
        out << def.pkgdir.string();
        for (auto const& pat: def.patterns_or) {
            out << ' ' << pat;
        }
        return out;
    }

    inline std::ostream&
    operator<< (std::ostream& out, config::definition const& def) {
        std::visit(
            [&out](auto const& d) {
                out << d;
            },
            def);
        return out;
    }

    inline std::ostream&
    operator<< (std::ostream& out, config const& conf) {
        for (auto const& def: conf) {
            out << def << std::endl;
        }
        return out;
    }
}
