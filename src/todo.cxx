#include <exception>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

#include "pkgdb.hxx"
#include "todo.hxx"

namespace {
    using namespace pkg_chk;

    auto const RE_PACKAGE_TODO = std::regex(
        // #1: PKGBASE
        // #2: PKGVERSION
        // #3: comment
        "^\\s*o ([^\\s]+?)-([0-9][^-\\s]*)(?:\\s+(.+))?$",
        std::regex::optimize);
}

namespace pkg_chk {
    todo_file::todo_file(std::filesystem::path const& file) {
        std::ifstream in(file, std::ios_base::in);
        if (!in) {
            throw std::system_error(errno, std::generic_category(), "Failed to open " + file.string());
        }
        in.exceptions(std::ios_base::badbit);

        for (std::string line; std::getline(in, line); ) {
            std::match_results<std::string::const_iterator> m;
            if (std::regex_match(line, m, RE_PACKAGE_TODO)) {
                pkgbase     base(m[1]);
                pkgversion  version(m[2]);
                std::string comment(m[3]);

                auto const it = find(m[1]);
                if (it == end() || it->second.name.version < version) {
                    emplace_hint(
                        it,
                        base,
                        todo_entry {
                            pkgname(base, std::move(version)),
                            std::move(comment)
                        });
                }
            }
        }
    }
}
