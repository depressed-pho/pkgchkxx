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

    build_info_iterator::build_info_iterator(
        std::string const& PKG_INFO,
        pkgxx::pkgpattern const& pattern)
        : _pkg_info(std::make_shared<pkgxx::harness>(
                        pkgxx::shell,
                        std::initializer_list<std::string>(
                            {pkgxx::shell, "-s", "--", "-Bq", pattern.string()}))) {

        _pkg_info->cin() << "exec " << PKG_INFO << " \"$@\"" << std::endl;
        _pkg_info->cin().close();

        ++(*this);
    }

    build_info_iterator&
    build_info_iterator::operator++ () {
        while (true) {
            if (std::getline(_pkg_info->cout(), _current_line)) {
                if (auto equal = _current_line.find('='); equal != std::string::npos) {
                    auto const line_v = std::string_view(_current_line);
                    _current.emplace(
                        line_v.substr(0, equal),
                        line_v.substr(equal + 1));
                    break;
                }
                else {
                    // Not a variable definition. Skip this line.
                    continue;
                }
            }
            else {
                _current.reset();
                break;
            }
        }
        return *this;
    }

    build_info::const_iterator
    build_info::find(std::string_view const& var) const {
        for (auto it = begin(); it != end(); it++) {
            if (it->first == var) {
                return it;
            }
        }
        return end();
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

            std::set<pkgxx::pkgname> ret;
            for (std::string line; std::getline(pkg_info.cout(), line); ) {
                if (line.empty()) {
                    break;
                }
                ret.emplace(line);
            }
            return ret;
        }

        std::set<pkgxx::pkgname>
        who_requires(std::string const& PKG_INFO, pkgxx::pkgpattern const& pat) {
            pkgxx::harness pkg_info(
                pkgxx::shell, {pkgxx::shell, "-s", "--", "-Rq", pat.string()});

            pkg_info.cin() << "exec " << PKG_INFO << " \"$@\"" << std::endl;
            pkg_info.cin().close();

            std::set<pkgxx::pkgname> ret;
            for (std::string line; std::getline(pkg_info.cout(), line); ) {
                if (line.empty()) {
                    break;
                }
                ret.emplace(line);
            }
            return ret;
        }
    }
}
