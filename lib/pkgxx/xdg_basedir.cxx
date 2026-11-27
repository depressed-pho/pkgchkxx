#include <stdexcept>
#include <utility>

#include "environment.hxx"

namespace pkgxx {
    std::filesystem::path const&
    HOME() {
        // As of C++11, static variables in a function scope are guaranteed
        // to be initialised at most once, even if multiple threads call
        // the function concurrently.
        static std::filesystem::path const cache = ([]() {
            auto env = cgetenv("HOME");
            if (env) {
                return std::filesystem::path{std::move(env.value())};
            }
            else {
                throw std::runtime_error("$HOME is not set");
            }
        })();
        return cache;
    }

    std::filesystem::path const&
    XDG_STATE_HOME() {
        static std::filesystem::path const cache = ([]() {
            auto env = cgetenv("XDG_STATE_HOME");
            if (env) {
                return std::filesystem::path{std::move(env.value())};
            }
            else {
                return HOME() / ".local" / "state";
            }
        })();
        return cache;
    }
}
