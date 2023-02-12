#include "config.h"

#include <array>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

#include "harness.hxx"

namespace {
    std::array<int, 2>
    cpipe() {
        std::array<int, 2> fds;
        if (pipe(fds.data()) != 0) {
            throw std::system_error(errno, std::generic_category(), "pipe");
        }
        return fds;
    }
}

namespace pkg_chk {
    harness::harness(
        std::string const& cmd,
        std::vector<std::string> const& argv,
        std::optional<std::string> const& cwd) {

        auto const stdin_fds  = cpipe();
        auto const stdout_fds = cpipe();

#if defined(HAVE_VFORK)
        _pid = vfork();
#else
        _pid = fork();
#endif
        if (_pid == 0) {
            dup2(stdin_fds[0], STDIN_FILENO);
            close(stdin_fds[1]);
            close(stdout_fds[0]);
            dup2(stdout_fds[1], STDOUT_FILENO);

            if (cwd) {
                // We can't use posix_spawn(3) because of this.
                if (chdir(cwd->c_str()) != 0) {
                    std::string const err
                        = "harness: Cannot chdir to " + *cwd + ": " + strerror(errno) + "\n";
                    write(STDERR_FILENO, err.c_str(), err.size());
                    _exit(1);
                }
            }

            std::vector<char const*> cargv;
            cargv.reserve(argv.size() + 1);
            for (auto const& arg: argv) {
                cargv.push_back(arg.c_str());
            }
            cargv.push_back(nullptr);

            if (execvp(cmd.c_str(), const_cast<char* const*>(cargv.data())) != 0) {
                std::string const err
                    = "harness: Cannot exec " + cmd + ": " + strerror(errno) + "\n";
                write(STDERR_FILENO, err.c_str(), err.size());
            }
            _exit(1);
        }
        else if (_pid > 0) {
            close(stdin_fds[0]);
            close(stdout_fds[1]);

            _stdin  = std::make_unique<fdostream>(stdin_fds[1]);
            _stdout = std::make_unique<fdistream>(stdout_fds[0]);

            _stdin->exceptions(std::ios_base::badbit);
            _stdout->exceptions(std::ios_base::badbit);
        }
        else {
            throw std::system_error(
                errno, std::generic_category(),
#if defined(HAVE_VFORK)
                "vfork"
#else
                "fork"
#endif
                );
        }
    }

    harness::~harness() noexcept(false) {
        int status;
        if (waitpid(_pid, &status, 0) == -1) {
            throw std::system_error(
                errno, std::generic_category(), "waitpid");
        }
    }
}
