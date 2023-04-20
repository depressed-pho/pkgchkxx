#include "pkgdb.hxx"

namespace pkgxx {
    installed_pkgname_iterator::installed_pkgname_iterator(std::string const& PKG_INFO)
        : _pkg_info(std::make_shared<pkgxx::harness>(
                        pkgxx::shell,
                        std::initializer_list<std::string>(
                            {pkgxx::shell, "-s", "--", "-e", "*"}))) {

        _pkg_info->cin() << "exec " << PKG_INFO << " \"$@\"" << std::endl;
        _pkg_info->cin().close();

        ++(*this);
    }

    installed_pkgname_iterator&
    installed_pkgname_iterator::operator++ () {
        std::string line;
        if (std::getline(_pkg_info->cout(), line) && !line.empty()) {
            _current.emplace(line);
        }
        else {
            _current.reset();
        }
        return *this;
    }

    namespace detail {
        bool
        is_pkg_installed(std::string const& PKG_INFO, pkgxx::pkgpattern const& pat) {
            pkgxx::harness pkg_info(
                pkgxx::shell, {pkgxx::shell, "-s", "--", "-q", "-e", pat.string()});

            pkg_info.cin() << "exec " << PKG_INFO << " \"$@\"" << std::endl;
            pkg_info.cin().close();

            return pkg_info.wait_exit().status == 0;
        }

        std::set<pkgxx::pkgname>
        build_depends(std::string const& PKG_INFO, pkgxx::pkgpattern const& pat) {
            pkgxx::harness pkg_info(
                pkgxx::shell, {pkgxx::shell, "-s", "--", "-Nq", pat.string()});

            pkg_info.cin() << "exec " << PKG_INFO << " \"$@\"" << std::endl;
            pkg_info.cin().close();

            std::set<pkgxx::pkgname> deps;
            for (std::string line; std::getline(pkg_info.cout(), line); ) {
                if (line.empty()) {
                    break;
                }
                deps.emplace(line);
            }
            return deps;
        }
    }
}
