#include "harness.hxx"
#include "pkgdb.hxx"

namespace pkgxx {
    namespace detail {
        std::map<std::string, std::string>
        build_info(std::string const& PKG_INFO, pkgxx::pkgpattern const& pat) {
            harness pkg_info(shell, {shell, "-s", "--", "-Bq", pat.string()});

            pkg_info.cin() << "exec " << PKG_INFO << " \"$@\"" << std::endl;
            pkg_info.cin().close();

            std::map<std::string, std::string> ret;
            for (std::string line; std::getline(pkg_info.cout(), line); ) {
                if (auto equal = line.find('='); equal != std::string::npos) {
                    ret.emplace(
                        line.substr(0, equal),
                        line.substr(equal + 1));
                }
                else {
                    // Not a variable definition. Skip this line.
                    continue;
                }
            }
            return ret;
        }

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

    std::set<pkgxx::pkgname>
    installed_pkgnames(std::string const& PKG_INFO) {
        harness pkg_info(shell, {shell, "-s", "--", "-e", "*"});

        pkg_info.cin() << "exec " << PKG_INFO << " \"$@\"" << std::endl;
        pkg_info.cin().close();

        std::set<pkgxx::pkgname> ret;
        for (std::string line; std::getline(pkg_info.cout(), line); ) {
            ret.emplace(line);
        }
        return ret;
    }
}
