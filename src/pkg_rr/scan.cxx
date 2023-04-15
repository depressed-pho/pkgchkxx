#include <pkgxx/harness.hxx>
#include <pkgxx/mutex_guard.hxx>
#include <pkgxx/nursery.hxx>
#include <pkgxx/pkgdb.hxx>
#include <pkgxx/string_algo.hxx>

#include "scan.hxx"

namespace pkg_rr {
    package_scanner::~package_scanner() noexcept(false) {
        pkgxx::nursery n(_concurrency);
        for (auto const& name: pkgxx::installed_pkgnames(_pkg_info)) {
            n.start_soon(
                [&, name = std::move(name)]() {
                    pkgxx::harness pkg_info(
                        pkgxx::shell, {pkgxx::shell, "-s", "--", "-Bq", name.string()});

                    pkg_info.cin() << "exec " << _pkg_info << " \"$@\"" << std::endl;
                    pkg_info.cin().close();

                    for (std::string line; std::getline(pkg_info.cout(), line); ) {
                        for (auto& axis: _axes) {
                            auto&       result  = std::get<1>(axis);
                            auto const& flag    = std::get<2>(axis);
                            auto const& exclude = std::get<3>(axis);

                            if (exclude.count(name.base) > 0) {
                                // The user wants it to be excluded from
                                // the result.
                                continue;
                            }
                            else if (line.size() == flag.size() + 4 &&
                                     std::string_view(line).substr(0, flag.size()) == flag &&
                                     line[flag.size()] == '=' &&
                                     pkgxx::ascii_tolower(line[flag.size()+1]) == 'y' &&
                                     pkgxx::ascii_tolower(line[flag.size()+1]) == 'e' &&
                                     pkgxx::ascii_tolower(line[flag.size()+1]) == 's') {

                                result.lock()->insert(name.base);
                            }
                        }
                    }
                });
        }
        for (auto& axis: _axes) {
            std::get<0>(axis).set_value(
                std::move(
                    *(std::get<1>(axis).lock())));
        }
    }
}
