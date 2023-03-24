#pragma once

#include <deque>
#include <filesystem>
#include <ostream>
#include <utility>
#include <variant>
#include <vector>

#include <pkgxx/pkgpath.hxx>

#include "tag.hxx"

namespace pkg_chk {
    struct config {
        /// Group definition line: TAG "=" *PATTERN
        struct group_def {
            group_def(std::string_view const& line);

            friend std::ostream&
            operator<< (std::ostream& out, config::group_def const& def);

            tag group;
            std::vector<tagpat> patterns_or;
        };

        /// Package definition line: PKGPATH *PATTERN
        struct pkg_def {
            template <typename Pkgpath, typename Patterns>
            pkg_def(Pkgpath&& path_, Patterns&& patterns_or_)
                : path(std::forward<Pkgpath>(path_))
                , patterns_or(std::forward<Patterns>(patterns_or_)) {}

            pkg_def(std::string_view const& line);

            friend std::ostream&
            operator<< (std::ostream& out, config::pkg_def const& def);

            pkgxx::pkgpath path;
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

        /** Obtain a set of pkgpaths in the config file, filtered by
         * applying tags. */
        std::set<pkgxx::pkgpath>
        pkgpaths(tagset const& included_tags = {},
                 tagset const& excluded_tags = {}) const;

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

        friend std::ostream&
        operator<< (std::ostream& out, config::definition const& def);

        friend std::ostream&
        operator<< (std::ostream& out, config const& conf);

    private:
        std::deque<definition> _defs;
    };
}
