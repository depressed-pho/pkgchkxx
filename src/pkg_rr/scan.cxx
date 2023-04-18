#include <cassert>

#include <pkgxx/harness.hxx>
#include <pkgxx/mutex_guard.hxx>
#include <pkgxx/nursery.hxx>
#include <pkgxx/pkgdb.hxx>
#include <pkgxx/string_algo.hxx>

#include "scan.hxx"

namespace pkg_rr {
    package_scanner::~package_scanner() noexcept(false) {
        {
            pkgxx::nursery n(_concurrency);
            for (auto const& name: pkgxx::installed_pkgnames(_pkg_info)) {
                n.start_soon(
                    [&, name = std::move(name)]() {
                        pkgxx::harness pkg_info(
                            pkgxx::shell, {pkgxx::shell, "-s", "--", "-Bq", name.string()});

                        pkg_info.cin() << "exec " << _pkg_info << " \"$@\"" << std::endl;
                        pkg_info.cin().close();

                        std::optional<pkgxx::pkgpath> path;
                        for (std::string line; std::getline(pkg_info.cout(), line); ) {
                            if (auto equal = line.find('='); equal != std::string::npos) {
                                auto const line_v = std::string_view(line);
                                auto const var    = line_v.substr(0, equal);
                                auto const value  = line_v.substr(equal + 1);

                                if (var == "PKGPATH") {
                                    path.emplace(value);
                                }
                                else {
                                    for (auto& axis: _axes) {
                                        auto&       result  = std::get<1>(axis);
                                        auto const& flag    = std::get<2>(axis);
                                        auto const& exclude = std::get<3>(axis);

                                        if (exclude.count(name.base) > 0) {
                                            // The user wants it to be excluded from
                                            // the result.
                                            continue;
                                        }
                                        else if (var == flag && pkgxx::ci_equal(value, "yes")) {
                                            assert(path.has_value());
                                            result.lock()->emplace(name.base, *path);
                                        }
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
