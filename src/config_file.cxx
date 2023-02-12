#include <algorithm>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>

#include "config_file.hxx"
#include "message.hxx"
#include "pkgdb.hxx"

namespace fs = std::filesystem;

namespace pkg_chk {
    void
    generate_conf_from_installed(options const& opts, environment const& env) {
        fs::path const& file = env.PKGCHK_CONF.get();
        verbose(opts) << "Write " << file << " based on installed packages" << std::endl;

        if (fs::exists(file)) {
            fs::path old = file;
            old += ".old";
            fs::rename(file, old);
        }

        std::ofstream out(file, std::ios_base::out | std::ios_base::trunc);
        out.exceptions(std::ios_base::badbit);

        std::time_t const now = std::time(nullptr);
        out << "# Generated automatically at "
            << std::put_time(std::localtime(&now), "%c %Z") << std::endl;

        std::deque<fs::path> pkgdirs;
        for (fs::path const& pkgdir: installed_pkgdirs(env)) {
            pkgdirs.push_back(pkgdir);
        }
        std::sort(pkgdirs.begin(), pkgdirs.end());

        config conf;
        for (fs::path const& pkgdir: pkgdirs) {
            conf.emplace_back(config::pkg_def(pkgdir, std::set<tagpat>()));
        }
        out << conf;
    }
}
