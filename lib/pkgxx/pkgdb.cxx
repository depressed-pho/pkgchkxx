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
}
