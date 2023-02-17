#pragma once

#include <functional>
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
    /* RAII way of spawning child processes.
     */
    struct harness {
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
            std::optional<std::string> const& cwd = std::nullopt) {

            init(cmd, argv, cwd, [](auto&) {});
        }

        template <typename EnvModifier>
        harness(
            std::string const& cmd,
            std::vector<std::string> const& argv,
            std::optional<std::string> const& cwd,
            EnvModifier&& env_mod) {

            static_assert(
                std::is_invocable_v<EnvModifier&&, std::map<std::string, std::string>&>);

            init(cmd, argv, cwd,
                 std::function<void (std::map<std::string, std::string>&)>(env_mod));
        }

        harness(harness const&) = delete;

        harness(harness&& other)
            : _pid(std::move(other._pid))
            , _stdin(std::move(other._stdin))
            , _stdout(std::move(other._stdout))
            , _status(std::move(other._status)) {

            other._pid.reset();
            other._stdin.reset();
            other._stdout.reset();
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

        status const&
        wait();

    private:
        void
        init(std::string const& cmd,
             std::vector<std::string> const& argv,
             std::optional<std::string> const& cwd,
             std::function<void (std::map<std::string, std::string>&)> const& env_mod);

        std::optional<pid_t> _pid;
        std::optional<fdostream> _stdin;
        std::optional<fdistream> _stdout;
        std::optional<status> _status;
    };
}
