#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

namespace pkgxx {
    namespace detail {
        // This is an incomplete type whose definition is in spawn.cxx
        struct file_actions;

        struct spawn_base {
        protected:
            template <typename Cmd, typename Argv>
            spawn_base(bool is_file, Cmd&& cmd, Argv&& argv)
                : _is_file(is_file)
                , _cmd(std::forward<Cmd>(cmd))
                , _argv(std::forward<Argv>(argv)) {}

        public:
            template <typename Env>
            spawn_base&
            environ(Env&& env) {
                _env.emplace(std::forward<Env>(env));
                return *this;
            }

            // These functions cannot be defined here, because they need to
            // interact with the incomplete type file_actions.

            spawn_base&
            chdir(std::filesystem::path const& dir);

            spawn_base&
            close_fd(int fd);

            spawn_base&
            dup_fd(int from, int to);

            pid_t
            operator() () const;

        private:
            file_actions&
            fas() const;

        private:
            bool _is_file;
            std::filesystem::path _cmd;
            std::vector<std::string> _argv;
            std::optional<
                std::map<std::string, std::string>
                > _env;
            // This can't be unique_ptr because then our constructor has to
            // be defined in spawn.cxx, which we don't want to do.
            mutable std::shared_ptr<file_actions> _fas;
        };
    }

    /** A wrapper function for pipe(2). */
    std::array<int, 2>
    cpipe(bool set_cloexec = false);

    /** Obtain the contents of \c environ(7) as a \c map.
     */
    std::map<std::string, std::string>
    cenviron();

    /** A wrapper for posix_spawn(2). You construct an object, call methods
     * to set options, then call \c operator() to spawn it. On platforms
     * where posix_spawn(2) is unavailable, it will be simulated with fork
     * & exec.
     */
    struct spawn: public detail::spawn_base {
        template <typename Path, typename Argv>
        spawn(Path&& path, Argv&& argv)
            : detail::spawn_base(
                false,
                std::forward<Path>(path),
                std::forward<Argv>(argv)) {}
    };

    /** A wrapper for posix_spawnp(2). See \ref spawn. */
    struct spawnp: public detail::spawn_base {
        template <typename File, typename Argv>
        spawnp(File&& file, Argv&& argv)
            : detail::spawn_base(
                true,
                std::forward<File>(file),
                std::forward<Argv>(argv)) {}
    };
}
