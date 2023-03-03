#pragma once

#include <filesystem>
#include <functional>
#include <future>
#include <istream>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <sys/types.h>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "fdstream.hxx"

namespace pkg_chk {
    static inline std::string const shell = "/bin/sh";

    /* RAII way of spawning child processes.
     */
    struct harness {
        enum class fd_action {
            inherit,
            close,
            pipe
        };

        struct exited {
            int status;
        };
        struct signaled {
            int signal;
            bool coredumped;
        };
        using status = std::variant<exited, signaled>;

        harness(
            std::string const& cmd,
            std::vector<std::string> const& argv,
            std::optional<std::filesystem::path> const& cwd = std::nullopt,
            std::function<void (std::map<std::string, std::string>&)> const& env_mod = [](auto&) {},
            fd_action stderr_action = fd_action::inherit);

        harness(harness const&) = delete;
        harness(harness&& other)
            : _pid(std::move(other._pid))
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

        ~harness() noexcept(false) {
            if (_pid) {
                wait();
            }
        }

        fdostream&
        cin() {
            return _stdin.value();
        }

        fdistream&
        cout() {
            return _stdout.value();
        }

        fdistream&
        cerr() {
            return _stderr.value();
        }

        /** Block until the spawned process terminates for any reason.
         */
        status const&
        wait();

        /** Block until the spawned process terminates. If it exits return
         * the status code, and if it dies of a signal throw
         * process_died_of_signal.
         */
        exited const&
        wait_exit();

        /** Block until the spawned process terminates. If it exits with
         * status 0 return normally, and if it terminates for any other
         * reasons throw either process_exited_for_failure or
         * process_died_of_signal. */
        void
        wait_success();

    private:
        // In
        std::string _cmd;
        std::vector<std::string> _argv;
        std::optional<std::filesystem::path> _cwd;
        std::map<std::string, std::string> _env;

        // Out
        std::optional<pid_t> _pid;
        std::optional<fdostream> _stdin;
        std::optional<fdistream> _stdout;
        std::optional<fdistream> _stderr;
        std::optional<status> _status;
    };

    struct command_error: std::runtime_error {
        command_error(
            std::string&& cmd_,
            std::vector<std::string>&& argv_,
            std::optional<std::filesystem::path>&& cwd_,
            std::map<std::string, std::string>&& env_);

        virtual char const*
        what() const noexcept {
            return msg.get().c_str();
        }

        std::string cmd;
        std::vector<std::string> argv;
        std::optional<std::filesystem::path> cwd;
        std::map<std::string, std::string> env;

    private:
        mutable std::shared_future<std::string> msg;
    };

    struct failed_to_spawn_process: command_error {
        failed_to_spawn_process(
            std::string&& msg_,
            std::string&& cmd_,
            std::vector<std::string>&& argv_,
            std::optional<std::filesystem::path>&& cwd_,
            std::map<std::string, std::string>&& env_);

        virtual char const*
        what() const noexcept {
            return msg.get().c_str();
        }

    private:
        mutable std::shared_future<std::string> msg;
    };

    struct process_terminated_unexpectedly: command_error {
        process_terminated_unexpectedly(
            pid_t pid_,
            std::string&& cmd_,
            std::vector<std::string>&& argv_,
            std::optional<std::filesystem::path>&& cwd_,
            std::map<std::string, std::string>&& env_);

        pid_t pid;
    };

    struct process_died_of_signal: public process_terminated_unexpectedly {
        process_died_of_signal(
            harness::signaled const& st_,
            pid_t pid_,
            std::string&& cmd_,
            std::vector<std::string>&& argv_,
            std::optional<std::filesystem::path>&& cwd_,
            std::map<std::string, std::string>&& env_);

        virtual char const*
        what() const noexcept {
            return msg.get().c_str();
        }

        harness::signaled st;

    private:
        mutable std::shared_future<std::string> msg;
    };

    struct process_exited_for_failure: public process_terminated_unexpectedly {
        process_exited_for_failure(
            harness::exited const& st_,
            pid_t pid_,
            std::string&& cmd_,
            std::vector<std::string>&& argv_,
            std::optional<std::filesystem::path>&& cwd_,
            std::map<std::string, std::string>&& env_);

        virtual char const*
        what() const noexcept {
            return msg.get().c_str();
        }

        harness::exited st;

    private:
        mutable std::shared_future<std::string> msg;
    };
}
