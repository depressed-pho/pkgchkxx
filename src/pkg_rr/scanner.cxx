#include <cassert>

#include <pkgxx/mutex_guard.hxx>
#include <pkgxx/nursery.hxx>
#include <pkgxx/pkgdb.hxx>
#include <pkgxx/string_algo.hxx>

#include "scanner.hxx"

namespace pkg_rr {
    package_scanner::~package_scanner() noexcept(false) {
        {
            pkgxx::nursery n(_concurrency);
            for (auto const& name: pkgxx::installed_pkgnames(_pkg_info)) {
                n.start_soon(
                    [&, name = std::move(name)]() {
                        std::optional<pkgxx::pkgpath> path;
                        for (auto const& [var, value]: pkgxx::build_info(_pkg_info, name)) {
                            if (var == "PKGPATH") {
                                path.emplace(value);
                            }
                            else {
                                for (auto& axis: _axes) {
                                    auto&       result  = std::get<1>(axis);
                                    auto const& flag    = std::get<2>(axis);
                                    auto const& exclude = std::get<3>(axis);

                                    if (exclude.count(name.base) > 0) {
                                        // The user wants the package to be
                                        // excluded from the result
                                        // regardless of what flags it has.
                                        continue;
                                    }
                                    else if (var == flag && pkgxx::ci_equal(value, "yes")) {
                                        assert(path.has_value());
                                        result.lock()->emplace(name.base, *path);
                                    }
                                }
                            }
                        }
                    });
            }
        }
        for (auto& axis: _axes) {
            std::get<0>(axis).set_value(
                std::move(
                    *(std::get<1>(axis).lock())));
        }
    }
}
