#include <fstream>
#include <future>
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

    auto const RE_PYTHON_PREFIX = std::regex(
        "^py[0-9]+-",
        std::regex::optimize);

    void
    normalize_pkgname(pkgname& name) {
        name.base = std::regex_replace(name.base, RE_PYTHON_PREFIX, "py-");
    }
}

namespace pkg_chk {
    todo_file::todo_file(std::filesystem::path const& file) {
        std::ifstream in(file, std::ios_base::in);
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

    void
    lookup_todo(environment const& env) {
        /* Spawning pkg_info(1) isn't instantaneous. Start parsing the TODO
         * file right now to save some time. */
        auto f_todo = std::async(
            std::launch::async,
            [env]() {
                return todo_file(env.PKGSRCDIR.get() / "doc/TODO");
            });

        installed_pkgnames pkgnames(env);
        todo_file const todo(f_todo.get());

        for (pkgname name: pkgnames) {
            normalize_pkgname(name);

            auto const it = todo.find(name.base);
            if (it != todo.end()) {
                std::cout << name.base << ": " << it->second.name;
                if (!it->second.comment.empty()) {
                    std::cout << " " << it->second.comment;
                }
                std::cout << std::endl;
            }
        }
    }
}
