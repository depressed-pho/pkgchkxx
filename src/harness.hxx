#pragma once

#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <sys/types.h>
#include <vector>

#include "fdstream.hxx"

namespace pkg_chk {
    /* RAII way of spawning child processes.
     */
    struct harness {
        harness(
            std::string const& cmd,
            std::vector<std::string> const& argv);

        ~harness() noexcept(false);

        fdostream&
        cin() const {
            return *_stdin;
        }

        fdistream&
        cout() const {
            return *_stdout;
        }

    private:
        pid_t _pid;
        std::unique_ptr<fdostream> _stdin;
        std::unique_ptr<fdistream> _stdout;
    };
}
