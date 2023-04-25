#include "config.h"

#include <array>
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
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
    cpipe(bool set_cloexec = false) {
        std::array<int, 2> fds;
        if (pipe(fds.data()) != 0) {
            throw std::system_error(errno, std::generic_category(), "pipe");
        }
        if (set_cloexec) {
            if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) == -1) {
                throw std::system_error(errno, std::generic_category(), "fcntl");
            }
            if (fcntl(fds[1], F_SETFD, FD_CLOEXEC) == -1) {
                throw std::system_error(errno, std::generic_category(), "fcntl");
            }
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

namespace pkgxx {
    harness::harness(
        std::string const& cmd,
        std::vector<std::string> const& argv,
        std::optional<std::filesystem::path> const& cwd,
        std::function<void (std::map<std::string, std::string>&)> const& env_mod,
        fd_action stdin_action,
        fd_action stdout_action,
        fd_action stderr_action)
        : _cmd(cmd)
        , _argv(argv)
        , _cwd(cwd)
        , _env(cenviron()) {

        env_mod(_env);
        std::vector<std::string> envp;
        for (auto const& [name, value]: _env) {
            envp.emplace_back(name + "=" + value);
        }

        auto const msg_fds    = cpipe(true);
        auto const stdin_fds  = stdin_action  == fd_action::pipe
            ? std::make_optional(cpipe(true))
            : std::nullopt;
        auto const stdout_fds = stdout_action == fd_action::pipe
            ? std::make_optional(cpipe(true))
            : std::nullopt;
        auto const stderr_fds = stderr_action == fd_action::pipe
            ? std::make_optional(cpipe(true))
            : std::nullopt;

#if defined(HAVE_VFORK)
        _pid = vfork();
#else
        _pid = fork();
#endif
        if (*_pid == 0) {
            close(msg_fds[0]);
            switch (stdin_action) {
            case fd_action::inherit:
                break;
            case fd_action::close:
                close(STDIN_FILENO);
                break;
            case fd_action::pipe:
                dup2((*stdin_fds)[0], STDIN_FILENO);
                break;
            default:
                std::string const err = "Invalid fd_action for stdin\n";
                write(msg_fds[1], err.c_str(), err.size());
                _exit(1);
            }
            switch (stdout_action) {
            case fd_action::inherit:
                break;
            case fd_action::close:
                close(STDOUT_FILENO);
                break;
            case fd_action::pipe:
                dup2((*stdout_fds)[1], STDOUT_FILENO);
                break;
            default:
                std::string const err = "Invalid fd_action for stdout\n";
                write(msg_fds[1], err.c_str(), err.size());
                _exit(1);
            }
            switch (stderr_action) {
            case fd_action::inherit:
                break;
            case fd_action::close:
                close(STDERR_FILENO);
                break;
            case fd_action::pipe:
                dup2((*stderr_fds)[1], STDERR_FILENO);
                break;
            case fd_action::merge_with_stdout:
                dup2(STDOUT_FILENO, STDERR_FILENO);
                break;
            default:
                std::string const err = "Invalid fd_action for stderr\n";
                write(msg_fds[1], err.c_str(), err.size());
                _exit(1);
            }

            if (cwd) {
                // We can't use posix_spawn(3) because of this.
                if (chdir(cwd->c_str()) != 0) {
                    std::string const err
                        = "Cannot chdir to " + cwd->string() + ": " + strerror(errno) + "\n";
                    write(msg_fds[1], err.c_str(), err.size());
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
                    = "Cannot exec " + cmd + ": " + strerror(errno) + "\n";
                write(msg_fds[1], err.c_str(), err.size());
            }
            _exit(1);
        }
        else if (*_pid > 0) {
            close(msg_fds[1]);
            if (stdin_fds) {
                close((*stdin_fds)[0]);
            }
            if (stdout_fds) {
                close((*stdout_fds)[1]);
            }
            if (stderr_fds) {
                close((*stderr_fds)[1]);
            }

            // The child will write an error message to this pipe if it
            // fails to exec.
            fdistream msg_in(msg_fds[0]);
            std::string msg;
            std::getline(msg_in, msg);
            if (!msg.empty()) {
                throw failed_to_spawn_process(
                    command_error(
                        std::move(_cmd),
                        std::move(_argv),
                        std::move(_cwd),
                        std::move(_env)),
                    std::move(msg));
            }

            if (stdin_fds) {
                _stdin.emplace((*stdin_fds)[1]);
                _stdin->exceptions(std::ios_base::badbit);
            }
            if (stdout_fds) {
                _stdout.emplace((*stdout_fds)[0]);
                _stdout->exceptions(std::ios_base::badbit);
            }
            if (stderr_fds) {
                _stderr.emplace((*stderr_fds)[0]);
                _stderr->exceptions(std::ios_base::badbit);
            }
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
        assert(_pid);

        if (!_status) {
            int cstatus;
            if (waitpid(*_pid, &cstatus, 0) == -1) {
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
                std::cerr << "The process " << *_pid << " terminated but it didn't exit nor receive a signal. "
                          << "Then what the hell has happened to it???" << std::endl;
                std::abort(); // Impossible
            }
        }

        return _status.value();
    }

    harness::exited const&
    harness::wait_exit() {
        return std::visit(
            [this](auto const& st) -> exited const& {
                if constexpr (std::is_same_v<harness::exited const&, decltype(st)>) {
                    return st;
                }
                else {
                    throw process_died_of_signal(
                        process_terminated_unexpectedly(
                            command_error(
                                std::move(_cmd),
                                std::move(_argv),
                                std::move(_cwd),
                                std::move(_env)),
                            _pid.value()),
                        st);
                }
            },
            wait());
    }

    void
    harness::wait_success() {
        if (exited const& st = wait_exit(); st.status != 0) {
            throw process_exited_for_failure(
                process_terminated_unexpectedly(
                    command_error(
                        std::move(_cmd),
                        std::move(_argv),
                        std::move(_cwd),
                        std::move(_env)),
                    _pid.value()),
                st);
        }
    }

#if !defined(DOXYGEN)
    command_error::command_error(
        std::string&& cmd_,
        std::vector<std::string>&& argv_,
        std::optional<std::filesystem::path>&& cwd_,
        std::map<std::string, std::string>&& env_)
        : std::runtime_error("")
        , cmd(std::move(cmd_))
        , argv(std::move(argv_))
        , cwd(std::move(cwd_))
        , env(std::move(env_))
        , msg(
            std::async(
                std::launch::deferred,
                [this]() {
                    return "Command arguments were: " + stringify_argv(argv);
                }).share()) {}
#endif

#if !defined(DOXYGEN)
    failed_to_spawn_process::failed_to_spawn_process(
        command_error&& ce,
        std::string&& msg_)
        : std::runtime_error("")
        , command_error(std::move(ce))
        , msg(
            std::async(
                std::launch::deferred,
                [this, msg_ = std::move(msg_)]() {
                    std::stringstream ss;
                    ss << "Failed to spawn command \"" << cmd << "\": " << msg_ << std::endl
                       << command_error::what();
                    return ss.str();
                }).share()) {}
#endif

#if !defined(DOXYGEN)
    process_terminated_unexpectedly::process_terminated_unexpectedly(
        command_error&& ce,
        pid_t pid_)
        : std::runtime_error("")
        , command_error(std::move(ce))
        , pid(pid_) {}
#endif

#if !defined(DOXYGEN)
    process_died_of_signal::process_died_of_signal(
        process_terminated_unexpectedly&& ptu,
        harness::signaled const& st_)
        : std::runtime_error("")
        , command_error(std::move(ptu))
        , process_terminated_unexpectedly(std::move(ptu))
        , st(st_)
        , msg(
            std::async(
                std::launch::deferred,
                [this]() {
                    std::stringstream ss;
                    ss << "Command \"" << cmd << "\" (pid " << pid << ") died of signal "
                       << strsignal(st.signal)
                       << (st.coredumped ? " (core dumped). " : ". ")
                       << process_terminated_unexpectedly::what();
                    return ss.str();
                }).share()) {}
#endif

#if !defined(DOXYGEN)
    process_exited_for_failure::process_exited_for_failure(
        process_terminated_unexpectedly&& ptu,
        harness::exited const& st_)
        : std::runtime_error("")
        , command_error(std::move(ptu))
        , process_terminated_unexpectedly(std::move(ptu))
        , st(st_)
        , msg(
            std::async(
                std::launch::deferred,
                [this]() {
                    std::stringstream ss;
                    ss << "Command \"" << cmd << "\" (pid " << pid << ") exited with status "
                       << st.status << ". "
                       << process_terminated_unexpectedly::what();
                    return ss.str();
                }).share()) {}
#endif
}
