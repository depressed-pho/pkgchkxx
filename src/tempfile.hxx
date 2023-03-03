#pragma once

#include <filesystem>
#include <tuple>

#include "fdstream.hxx"

namespace pkg_chk {
    struct tempfile {
        enum class unlink_mode {
            immediately,
            on_destruction,
            never
        };

        tempfile(unlink_mode ul_mode_ = unlink_mode::on_destruction);
        virtual ~tempfile();

        unlink_mode ul_mode;
        std::filesystem::path const path;
        fdstream ios;

    private:
        tempfile(
            unlink_mode ul_mode_,
            std::tuple<std::filesystem::path, fdstream>&& tmp);
    };
}
