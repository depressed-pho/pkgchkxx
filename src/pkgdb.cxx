#include "pkgdb.hxx"

namespace {
    std::string const shell = "/bin/sh";
}

namespace pkg_chk {
    installed_pkgnames::installed_pkgnames(environment const& env)
        : _pkg_info(shell, {shell, "-s"}) {

        _pkg_info.cin() << "exec " << env.PKG_INFO.get() << std::endl;
        _pkg_info.cin().close();
    }

    std::optional<installed_pkgnames::value_type>
    installed_pkgnames::read_next() {
        std::string line;
        if (std::getline(_pkg_info.cout(), line)) {
            auto const spc = line.find_first_of(" \t");
            if (spc == std::string::npos) {
                // This shouldn't happen.
                return read_next();
            }
            else {
                return pkgname(line.substr(0, spc));
            }
        }
        else {
            return std::nullopt;
        }
    }

    installed_pkgdirs::installed_pkgdirs(environment const& env)
        : _pkg_info(shell, {shell, "-s", "--", "-aQ", "PKGPATH"}) {

        _pkg_info.cin() << "exec " << env.PKG_INFO.get() << " \"$@\"" << std::endl;
        _pkg_info.cin().close();
    }

    std::optional<installed_pkgdirs::value_type>
    installed_pkgdirs::read_next() {
        std::string line;
        if (std::getline(_pkg_info.cout(), line)) {
            if (line.empty()) {
                // This shouldn't happen.
                return read_next();
            }
            else {
                return line;
            }
        }
        else {
            return std::nullopt;
        }
    }
}
