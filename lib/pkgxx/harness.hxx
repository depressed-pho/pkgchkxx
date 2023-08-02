#pragma once

#include <filesystem>
#include <functional>
#include <istream>
#include <map>
#include <optional>
#include <ostream>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <pkgxx/fdstream.hxx>

namespace pkgxx {
    static inline std::string const shell = "/bin/sh";

    template <typename Argv>
    inline std::string
    stringify_argv(Argv const& argv) {
        using namespace std::literals;
        static auto const any_specials    = " \t\n~`#$&*()\\|[];'\"<>?";
        static auto const strong_specials = "$`\\\""sv;

        std::stringstream ss;
        bool is_first = true;
        for (auto const& arg: argv) {
            if (is_first) {
                is_first = false;
            }
            else {
                ss << ' ';
            }
            if (arg.find_first_of(any_specials) != Argv::value_type::npos) {
                // The argument contains some shell special
                // characters. Quote it to not confuse someone seeing this
                // message.
                ss << '"';
                for (auto c: arg) {
                    if (strong_specials.find(c) != std::string_view::npos) {
                        ss << '\\';
                    }
                    ss << c;
                }
                ss << '"';
            }
            else {
                ss << arg;
            }
        }
        return ss.str();
    }

    /** RAII way of spawning child processes.
     */
    struct harness {
        /** An enum class to specify what to do upon destructing a
         * harness.
         */
        enum class dtor_action {
            /** Call wait() on destruction. */
            wait,
            /** Call wait_success() on destruction. This is the default. */
            wait_success,
            /** Call kill(SIGTERM) and wait() on destruction. */
            kill
        };

        /** An enum class to specify what to do about a file descriptor.
         */
        enum class fd_action {
            inherit,
            close,
            pipe,
            merge_with_stdout ///< Only makes sense for stderr.
        };

        /** The child process terminated normally by a call to \c _Exit(2)
         * or \c exit(3).
         */
        struct exited {
            int status;
        };
        /** The child process terminated due to a receipt of a signal.
         */
        struct signaled {
            int signal;
            bool coredumped;
        };
        /** A status of a terminated process. */
        using status = std::variant<exited, signaled>;

        /** Spawn a child process. The command \c cmd should either be a
         * path to an executable file or a name of command found in the
         * environment variable \c PATH.
         */
        harness(
            std::string const& cmd,
            std::vector<std::string> const& argv,
            std::optional<std::filesystem::path> const& cwd = std::nullopt,
            std::function<void (std::map<std::string, std::string>&)> const& env_mod = [](auto&) {},
            std::optional<dtor_action> da = std::nullopt,
            fd_action stdin_action  = fd_action::pipe,
            fd_action stdout_action = fd_action::pipe,
            fd_action stderr_action = fd_action::inherit);

        harness(harness const&) = delete;

        /** Construct a \ref harness by moving a process out of another
         * instance. The instance \c other will become invalidated. */
        harness(harness&& other);

        /** Wait until the spawned process terminates unless the \ref
         * harness has been invalidated. If none of wait(), wait_exit(),
         * wait_success() has been called, the destructor by default calls
         * wait_success() and throws an exception if the process doesn't
         * exit with status zero. This behavior is configurable with
         * dtor_action passed to the constructor.
         */
        ~harness() noexcept(false);

        /** Obtain a reference to an output stream that corresponds to the
         * standard input of the child process, or throw an exception if
         * the harness has been invalidated.
         */
        fdostream&
        cin() {
            return _stdin.value();
        }

        /** Obtain a reference to an input stream that corresponds to the
         * standard output of the child process, or throw an exception if
         * the harness has been invalidated.
         */
        fdistream&
        cout() {
            return _stdout.value();
        }

        /** Obtain a reference to an input stream that corresponds to the
         * standard error of the child process, or throw an exception if
         * the harness has been invalidated or the stderr isn't piped.
         */
        fdistream&
        cerr() {
            return _stderr.value();
        }

        /** Send a signal to the spawned process.
         */
        void
        kill(int sig = SIGTERM);

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
        dtor_action _da;

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

    /** An error happened while running an external command. */
    struct command_error: virtual std::runtime_error {
#if !defined(DOXYGEN)
        command_error(
            std::string&& cmd_,
            std::vector<std::string>&& argv_,
            std::optional<std::filesystem::path>&& cwd_,
            std::map<std::string, std::string>&& env_);
#endif

        /// Obtain a string representation of the error.
        virtual char const*
        what() const noexcept override;

        /// A name or a path to the command.
        std::string cmd;
        /// A vector of arguments to the command.
        std::vector<std::string> argv;
        /// The working directory for the command.
        std::optional<std::filesystem::path> cwd;
        /// Environmental variables for the command.
        std::map<std::string, std::string> env;

    private:
        mutable std::optional<std::string> msg;
    };

    /** An error happened while trying to spawn a process. */
    struct failed_to_spawn_process: virtual command_error {
#if !defined(DOXYGEN)
        failed_to_spawn_process(
            command_error&& ce,
            std::string&& reason_);
#endif

        virtual char const*
        what() const noexcept override;

    private:
        std::string reason;
        mutable std::optional<std::string> msg;
    };

    /** A child process terminated in an unexpected way. */
    struct process_terminated_unexpectedly: virtual command_error {
#if !defined(DOXYGEN)
        process_terminated_unexpectedly(
            command_error&& ce,
            pid_t pid_);
#endif

        /// The pid of the child process.
        pid_t pid;
    };

    /** A child process unexpectedly died of a signal. */
    struct process_died_of_signal: virtual process_terminated_unexpectedly {
#if !defined(DOXYGEN)
        process_died_of_signal(
            process_terminated_unexpectedly&& ptu,
            harness::signaled const& st_);
#endif

        virtual char const*
        what() const noexcept override;

        /// The signal which caused the process to terminate.
        harness::signaled st;

    private:
        mutable std::optional<std::string> msg;
    };

    /** A child process unexpectedly terminated for a reason other than
     * calling \c _Exit(2) or \c exit(3) with an argument \c 0.
     */
    struct process_exited_for_failure: virtual process_terminated_unexpectedly {
#if !defined(DOXYGEN)
        process_exited_for_failure(
            process_terminated_unexpectedly&& ptu,
            harness::exited const& st_);
#endif

        virtual char const*
        what() const noexcept override;

        /// The exit status of the process.
        harness::exited st;

    private:
        mutable std::optional<std::string> msg;
    };
}
