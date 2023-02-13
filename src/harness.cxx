#include "config.h"

#include <array>
#include <cerrno>
#include <string.h>
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

    std::map<std::string, std::string>
    cenviron() {
        std::map<std::string, std::string> env_map;
        for (char** ep = environ; *ep; ep++) {
            std::string const es = *ep;
            auto const equal = es.find('=');
            if (equal != std::string::npos) {
                env_map.emplace(es.substr(0, equal), es.substr(equal + 1));
            }
        }
        return env_map;
    }
}

namespace pkg_chk {
    harness::harness(
        std::string const& cmd,
        std::vector<std::string> const& argv,
        environ_modifier const& env_mod,
        std::optional<std::string> const& cwd) {

        auto env_map = cenviron();
        env_mod(env_map);
        std::vector<std::string> envp;
        for (auto const& env_pair: env_map) {
            envp.emplace_back(env_pair.first + "=" + env_pair.second);
        }

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

            std::vector<char const*> cenvp;
            cenvp.reserve(envp.size() + 1);
            for (auto const& env: envp) {
                cenvp.push_back(env.c_str());
            }
            cenvp.push_back(nullptr);

            if (execvpe(
                    cmd.c_str(),
                    const_cast<char* const*>(cargv.data()),
                    const_cast<char* const*>(cenvp.data())) != 0) {
                std::string const err
                    = "harness: Cannot exec " + cmd + ": " + strerror(errno) + "\n";
                write(STDERR_FILENO, err.c_str(), err.size());
            }
            _exit(1);
        }
        else if (_pid > 0) {
            close(stdin_fds[0]);
            close(stdout_fds[1]);

            _stdin.emplace(stdin_fds[1]);
            _stdout.emplace(stdout_fds[0]);

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

    harness::status const&
    harness::wait() {
        if (!_status) {
            int cstatus;
            if (waitpid(_pid, &cstatus, 0) == -1) {
                throw std::system_error(
                    errno, std::generic_category(), "waitpid");
            }
            else if (WIFEXITED(cstatus)) {
                _status.emplace(exited {WEXITSTATUS(cstatus)});
            }
            else if (WIFSIGNALED(cstatus)) {
                _status.emplace(signaled {WTERMSIG(cstatus), WCOREDUMP(cstatus)});
            }
            else {
                std::abort(); // Impossible
            }
        }
        return _status.value();
    }
}
