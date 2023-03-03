#include <cerrno>
#include <system_error>
#include <stdlib.h>

#include "tempfile.hxx"

using namespace pkg_chk;
namespace fs = std::filesystem;

namespace {
    std::tuple<fs::path, fdstream>
    cmkstemp(fs::path const& tmpl) {
        // mkstemp(3) tells us the name of file it creates by overwriting
        // the template.
        std::string tmpl_str = tmpl.string();
        if (int fd = mkstemp(tmpl_str.data()); fd > -1) {
            return std::make_tuple(
                fs::path(std::move(tmpl_str)),
                fdstream(fd));
        }
        else {
            throw std::system_error(errno, std::generic_category(), "mkstemp");
        }
    }
}

namespace pkg_chk {
    tempfile::tempfile(unlink_mode ul_mode_)
        : tempfile(ul_mode_, cmkstemp(fs::temp_directory_path() / "temp.XXXXXX")) {}

    tempfile::~tempfile() {
        if (ul_mode == unlink_mode::on_destruction) {
            fs::remove(path);
        }
    }

    tempfile::tempfile(
        unlink_mode ul_mode_,
        std::tuple<std::filesystem::path, fdstream>&& tmp)
        : ul_mode(ul_mode_)
        , path(std::get<0>(std::move(tmp)))
        , ios(std::get<1>(std::move(tmp))) {

        if (ul_mode == unlink_mode::immediately) {
            fs::remove(path);
        }
    }
}
