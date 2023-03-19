#include "config.h"

#include <fstream>
#include <sstream>
#include <type_traits>
#include <utility>

#include "build_version.hxx"
#include "harness.hxx"
#include "tempfile.hxx"

using namespace pkg_chk;
namespace fs = std::filesystem;

namespace {
    build_version
    read_build_version(std::istream& in) {
        build_version bv;
        for (std::string line; std::getline(in, line); ) {
            if (line.empty()) {
                break;
            }
            else if (auto const sep = line.find(": "); sep != std::string::npos) {
                fs::path const file = line.substr(0, sep);

                auto const tag_begin = line.find_first_not_of(' ', sep + 2);
                std::string const tag = tag_begin == std::string::npos ? "" : line.substr(tag_begin);

                bv.insert_or_assign(std::move(file), std::move(tag));
            }
        }
        return bv;
    }
}

namespace pkg_chk {
    std::optional<build_version>
    build_version::from_binary(
        std::string const& PKG_INFO,
        std::filesystem::path const& bin_pkg_file) {

        if (!fs::exists(bin_pkg_file)) {
            return {};
        }

        harness pkg_info(shell, {shell, "-s", "--", "-q", "-b", bin_pkg_file});
        pkg_info.cin() << "exec " << PKG_INFO << " \"$@\"" << std::endl;
        pkg_info.cin().close();

        build_version const bv = read_build_version(pkg_info.cout());
        if (pkg_info.wait_exit().status == 0) {
            return bv;
        }
        else {
            return {};
        }
    }

    std::optional<build_version>
    build_version::from_installed(
        std::string const& PKG_INFO,
        pkgname const& name) {

        // Discard stderr because the package might not be installed. This
        // is the only way to suppress errors in that case.
        harness pkg_info(
            shell,
            {shell, "-s", "--", "-q", "-b", name.string()},
            std::nullopt,
            [](auto&) {},
            harness::fd_action::close);
        pkg_info.cin() << "exec " << PKG_INFO << " \"$@\"" << std::endl;
        pkg_info.cin().close();

        build_version const bv = read_build_version(pkg_info.cout());
        if (pkg_info.wait_exit().status == 0) {
            return bv;
        }
        else {
            return {};
        }
    }

    std::optional<build_version>
    build_version::from_source(
        std::filesystem::path const& PKGSRCDIR,
        pkgpath const& path) {

        if (!fs::exists(PKGSRCDIR / path)) {
            return {};
        }

        // Unfortunately pkgsrc always outputs to a file, but it does
        // helpfully allows us to specify the name
        tempfile tmp;
        std::vector<std::string> const argv = {
            CFG_BMAKE,
            "_BUILD_VERSION_FILE=" + tmp.path.string(),
            tmp.path.string()
        };

        // But if the file already exists pkgsrc won't overwrite it, saying
        // "'/tmp/temp.XXXXXX' is up to date". This means we have to unlink
        // the temporary file and then reopen it after make(1) exits.
        fs::remove(tmp.path);
        harness(CFG_BMAKE, argv, PKGSRCDIR / path).wait_success();

        std::ifstream in(tmp.path, std::ios_base::in);
        if (!in) {
            throw std::system_error(
                errno, std::generic_category(), "Failed to reopen " + tmp.path.string());
        }
        in.exceptions(std::ios_base::badbit);

        return read_build_version(in);
    }

    std::ostream&
    operator<< (std::ostream& out, build_version const& bv) {
        for (auto const& pair: bv) {
            out << pair.first.string() << ": " << pair.second << std::endl;
        }
        return out;
    }
}
