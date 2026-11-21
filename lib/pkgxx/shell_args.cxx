#include <sstream>
#include <utility>

#include "shell_args.hxx"

namespace pkgxx {
#if !defined(DOXYGEN)
    bad_shell_args::bad_shell_args(std::string_view const& str)
        : std::runtime_error("")
        , str(str) {}
#endif

    char const*
    bad_shell_args::what() const noexcept {
        if (!msg) {
            msg.emplace("Malformed shell arguments: " + str);
        }
        return msg->c_str();
    }

#if !defined(DOXYGEN)
    unmatched_quoted_string::unmatched_quoted_string(
        bad_shell_args&& bsa,
        std::string::size_type pos)
        : std::runtime_error("")
        , bad_shell_args(std::move(bsa))
        , pos(pos) {}
#endif

    char const*
    unmatched_quoted_string::what() const noexcept {
        if (!msg) {
            std::stringstream ss;
            ss << bad_shell_args::what() << ": unmatched quote at position " << pos;
            msg.emplace(ss.str());
        }
        return msg->c_str();
    }

#if !defined(DOXYGEN)
    orphan_backslash::orphan_backslash(
        bad_shell_args&& bsa,
        std::string::size_type pos)
        : std::runtime_error("")
        , bad_shell_args(std::move(bsa))
        , pos(pos) {}
#endif

    char const*
    orphan_backslash::what() const noexcept {
        if (!msg) {
            std::stringstream ss;
            ss << bad_shell_args::what() << ": orphan backslash at position " << pos;
            msg.emplace(ss.str());
        }
        return msg->c_str();
    }
}
