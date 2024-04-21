#include <cassert>
#include <cerrno>
#include <iostream>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

#include "harness.hxx"
#include "spawn.hxx"

namespace pkgxx {
    harness::harness(
        int,
        std::filesystem::path const& cmd,
        std::vector<std::string> const& argv,
        std::optional<std::filesystem::path> const& cwd,
        std::function<void (std::map<std::string, std::string>&)> const& env_mod,
        dtor_action da,
        fd_action stdin_action,
        fd_action stdout_action,
        fd_action stderr_action)
        : _da(da)
        , _cmd(cmd)
        , _argv(argv)
        , _cwd(cwd)
        , _env(cenviron()) {

        env_mod(_env);

        auto const stdin_fds  = stdin_action  == fd_action::pipe
            ? std::make_optional(cpipe(true))
            : std::nullopt;
        auto const stdout_fds = stdout_action == fd_action::pipe
            ? std::make_optional(cpipe(true))
            : std::nullopt;
        auto const stderr_fds = stderr_action == fd_action::pipe
            ? std::make_optional(cpipe(true))
            : std::nullopt;

        spawnp s(cmd, argv);
        s.environ(_env);

        if (cwd) {
            s.chdir(*cwd);
        }

        switch (stdin_action) {
        case fd_action::inherit:
            break;
        case fd_action::close:
            s.close_fd(STDIN_FILENO);
            break;
        case fd_action::pipe:
            s.dup_fd((*stdin_fds)[0], STDIN_FILENO);
            break;
        default:
            assert(0 && "must not reach here");
            std::abort();
        }

        switch (stdout_action) {
        case fd_action::inherit:
            break;
        case fd_action::close:
            s.close_fd(STDOUT_FILENO);
            break;
        case fd_action::pipe:
            s.dup_fd((*stdout_fds)[1], STDOUT_FILENO);
            break;
        default:
            assert(0 && "must not reach here");
            std::abort();
        }

        switch (stderr_action) {
        case fd_action::inherit:
            break;
        case fd_action::close:
            s.close_fd(STDERR_FILENO);
            break;
        case fd_action::pipe:
            s.dup_fd((*stderr_fds)[1], STDERR_FILENO);
            break;
        case fd_action::merge_with_stdout:
            s.dup_fd(STDOUT_FILENO, STDERR_FILENO);
            break;
        default:
            assert(0 && "must not reach here");
            std::abort();
        }

        try {
            _pid = s();
        }
        catch (std::exception& e) {
            throw failed_to_spawn_process(
                command_error(
                    std::move(_cmd),
                    std::move(_argv),
                    std::move(_cwd),
                    std::move(_env)),
                e.what());
        }

        if (stdin_fds) {
            close((*stdin_fds)[0]);
            _stdin.emplace((*stdin_fds)[1]);
            _stdin->exceptions(std::ios_base::badbit);
        }
        if (stdout_fds) {
            close((*stdout_fds)[1]);
            _stdout.emplace((*stdout_fds)[0]);
            _stdout->exceptions(std::ios_base::badbit);
        }
        if (stderr_fds) {
            close((*stderr_fds)[1]);
            _stderr.emplace((*stderr_fds)[0]);
            _stderr->exceptions(std::ios_base::badbit);
        }
    }

    harness::harness(harness&& other)
        : _da(other._da)
        , _pid(std::move(other._pid))
        , _stdin(std::move(other._stdin))
        , _stdout(std::move(other._stdout))
        , _stderr(std::move(other._stderr))
        , _status(std::move(other._status)) {

        other._pid.reset();
        other._stdin.reset();
        other._stdout.reset();
        other._stderr.reset();
        other._status.reset();
    }

    harness::~harness() noexcept(false) {
        if (_pid && !_status) {
            switch (_da) {
            case dtor_action::wait:
                wait();
                break;

            case dtor_action::wait_success:
                wait_success();
                break;

            case dtor_action::kill:
                kill(SIGTERM);
                wait();
                break;

            default:
                assert(0 && "must not reach here");
                std::abort();
            }
        }
    }

    void
    harness::kill(int sig) {
        assert(_pid);

        if (!_status) {
            if (::kill(*_pid, sig) == -1) {
                if (errno == ESRCH) {
                    // The process has already gone. This is not an error.
                }
                else {
                    throw std::system_error(
                        errno, std::generic_category(), "kill");
                }
            }
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
                _status.emplace(signaled {
                    WTERMSIG(cstatus),
                    static_cast<bool>(WCOREDUMP(cstatus))});
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
        std::filesystem::path&& cmd_,
        std::vector<std::string>&& argv_,
        std::optional<std::filesystem::path>&& cwd_,
        std::map<std::string, std::string>&& env_)
        : std::runtime_error("")
        , cmd(std::move(cmd_))
        , argv(std::move(argv_))
        , cwd(std::move(cwd_))
        , env(std::move(env_)) {}
#endif

    char const*
    command_error::what() const noexcept {
        if (!msg) {
            msg.emplace(
                "Command arguments were: " + stringify_argv(argv));
        }
        return msg->c_str();
    }

#if !defined(DOXYGEN)
    failed_to_spawn_process::failed_to_spawn_process(
        command_error&& ce,
        std::string&& reason_)
        : std::runtime_error("")
        , command_error(std::move(ce))
        , reason(std::move(reason_)) {}
#endif

    char const*
    failed_to_spawn_process::what() const noexcept {
        if (!msg) {
            std::stringstream ss;
            ss << "Failed to spawn command \"" << cmd << "\": " << reason << std::endl
               << command_error::what();
            msg.emplace(ss.str());
        }
        return msg->c_str();
    }

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
        , st(st_) {}
#endif

    char const*
    process_died_of_signal::what() const noexcept {
        if (!msg) {
            std::stringstream ss;
            ss << "Command \"" << cmd << "\" (pid " << pid << ") died of signal "
               << strsignal(st.signal)
               << (st.coredumped ? " (core dumped). " : ". ")
               << process_terminated_unexpectedly::what();
            msg.emplace(ss.str());
        }
        return msg->c_str();
    }

#if !defined(DOXYGEN)
    process_exited_for_failure::process_exited_for_failure(
        process_terminated_unexpectedly&& ptu,
        harness::exited const& st_)
        : std::runtime_error("")
        , command_error(std::move(ptu))
        , process_terminated_unexpectedly(std::move(ptu))
        , st(st_) {}
#endif

    char const*
    process_exited_for_failure::what() const noexcept {
        if (!msg) {
            std::stringstream ss;
            ss << "Command \"" << cmd << "\" (pid " << pid << ") exited with status "
               << st.status << ". "
               << process_terminated_unexpectedly::what();
            msg.emplace(ss.str());
        }
        return msg->c_str();
    }
}
