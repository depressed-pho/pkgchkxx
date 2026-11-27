#pragma once

#include <filesystem>

namespace pkgxx {
    /// Return \c $HOME, or throw \c std::runtime_error if it's unset or
    /// empty.
    std::filesystem::path const&
    HOME();

    /// Return \c $XDG_STATE_HOME, or return \c $HOME/.local/state if it's
    /// unset or empty.
    std::filesystem::path const&
    XDG_STATE_HOME();
}
