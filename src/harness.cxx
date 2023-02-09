#include <array>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

#include "harness.hxx"

extern "C" {
    extern char** environ;
}

namespace {
    std::array<int, 2>
    cpipe() {
        std::array<int, 2> fds;
        if (pipe(fds.data()) != 0) {
            throw std::system_error(errno, std::generic_category(), "pipe");
        }
        return fds;
    }

    struct pspawn_file_actions {
        pspawn_file_actions() {
            if ((errno = posix_spawn_file_actions_init(&_actions)) != 0) {
                throw std::system_error(
                    errno, std::generic_category(), "posix_spawn_file_actions_init");
            }
        }

        ~pspawn_file_actions() noexcept(false) {
            if ((errno = posix_spawn_file_actions_destroy(&_actions)) != 0) {
                throw std::system_error(
                    errno, std::generic_category(), "posix_spawn_file_actions_destroy");
            }
        }

        void
        add_dup2(int old_fd, int new_fd) {
            if ((errno = posix_spawn_file_actions_adddup2(&_actions, old_fd, new_fd)) != 0) {
                throw std::system_error(
                    errno, std::generic_category(), "posix_spawn_file_actions_adddup2");
            }
        }

        void
        add_close(int fd) {
            if ((errno = posix_spawn_file_actions_addclose(&_actions, fd)) != 0) {
                throw std::system_error(
                    errno, std::generic_category(), "posix_spawn_file_actions_addclose");
            }
        }

        operator posix_spawn_file_actions_t const*() const {
            return &_actions;
        }

    private:
        posix_spawn_file_actions_t _actions;
    };

    pid_t
    cposix_spawnp(
        std::string const& cmd,
        pspawn_file_actions const& actions,
        std::vector<std::string> const& argv) {

        std::vector<char const*> cargv;
        cargv.reserve(argv.size() + 1);
        for (auto const& arg: argv) {
            cargv.push_back(arg.c_str());
        }
        cargv.push_back(nullptr);

        pid_t pid;
        if ((errno = posix_spawnp(
                 &pid,
                 cmd.c_str(),
                 actions,
                 NULL,
                 const_cast<char* const*>(cargv.data()),
                 environ)) != 0) {
            throw std::system_error(
                errno, std::generic_category(), "posix_spawnp");
        }
        return pid;
    }
}

namespace pkg_chk {
    harness::harness(
        std::string const& cmd,
        std::vector<std::string> const& argv) {

        auto const stdin_fds  = cpipe();
        auto const stdout_fds = cpipe();

        pspawn_file_actions actions;
        actions.add_dup2(stdin_fds[0], STDIN_FILENO);
        actions.add_close(stdin_fds[1]);
        actions.add_close(stdout_fds[0]);
        actions.add_dup2(stdout_fds[1], STDOUT_FILENO);

        _pid = cposix_spawnp(cmd, actions, argv);

        close(stdin_fds[0]);
        close(stdout_fds[1]);

        _stdin  = std::unique_ptr<fdostream>(new fdostream(stdin_fds[1]));
        _stdout = std::unique_ptr<fdistream>(new fdistream(stdout_fds[0]));
    }

    harness::~harness() noexcept(false) {
        int status;
        if (waitpid(_pid, &status, 0) == -1) {
            throw std::system_error(
                errno, std::generic_category(), "waitpid");
        }
    }
}
