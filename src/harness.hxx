#pragma once

#include <functional>
#include <istream>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <sys/types.h>
#include <utility>
#include <variant>
#include <vector>

#include "fdstream.hxx"

namespace pkg_chk {
    /* RAII way of spawning child processes.
     */
    struct harness {
        using environ_modifier =
            std::function<void (std::map<std::string, std::string>& env_map)>;
        static inline environ_modifier const nop_modifier = [](auto&) {};

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
            environ_modifier const& env_mod = nop_modifier,
            std::optional<std::string> const& cwd = std::nullopt);

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
        std::optional<pid_t> _pid;
        std::optional<fdostream> _stdin;
        std::optional<fdistream> _stdout;
        std::optional<status> _status;
    };
}
