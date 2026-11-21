#pragma once

#include <stdexcept>
#include <functional>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace pkgxx {
    struct bad_shell_args: virtual std::runtime_error {
#if !defined(DOXYGEN)
        bad_shell_args(std::string_view const& str);
#endif

        virtual char const*
        what() const noexcept override;

        /// The given string that was malformed.
        std::string str;

    private:
        mutable std::optional<std::string> msg;
    };

    struct unmatched_quoted_string: virtual bad_shell_args {
#if !defined(DOXYGEN)
        unmatched_quoted_string(
            bad_shell_args&& bsa,
            std::string::size_type pos);
#endif

        virtual char const*
        what() const noexcept override;

    private:
        mutable std::optional<std::string> msg;
        std::string::size_type pos;
    };

    struct orphan_backslash: virtual bad_shell_args {
#if !defined(DOXYGEN)
        orphan_backslash(
            bad_shell_args&& bsa,
            std::string::size_type pos);
#endif

        virtual char const*
        what() const noexcept override;

    private:
        mutable std::optional<std::string> msg;
        std::string::size_type pos;
    };

    /** \c shell_args represents a sequence of arguments to a command. It's
     * usually just a \c std::vector<std::string>, but with an additional
     * capability to convert from/to a single string, with each argument
     * being possibly quoted.
     */
    template <template <typename> typename Seq = std::vector>
    struct shell_args: public Seq<std::string> {
        /** Construct a \c shell_args by splitting words in a single
         * string.
         */
        shell_args(std::string_view const& str);

        /** Construct a \c shell_args from an initializer list of
         * arguments.
         */
        shell_args(std::initializer_list<std::string> from)
            : Seq<std::string>(from) {}

        /** Construct a \c shell_args by copying an argument list.
         */
        shell_args(Seq<std::string> const& from)
            : Seq<std::string>(from) {}

        /** Construct a \c shell_args by moving from an argument list.
         */
        shell_args(Seq<std::string>&& from)
            : Seq<std::string>(std::move(from)) {}

        /** Produce a single string representing the arguments that could
         * be parsed by any POSIX-compliant shells.
         */
        std::string
        string() const;

        /** Implicit conversion to std::string is equivalent to calling
         * string().
         */
        operator std::string () const {
            return string();
        }
    };

    /** Streaming an instance of \c shell_args will output a string
     * representation of the arguments, i.e. it is as if string() were
     * called internally.
     */
    template <template <typename> typename Seq>
    std::ostream&
    operator<< (std::ostream& out, shell_args<Seq> const& sa);

    // Implementation

    template <template <typename> typename Seq>
    shell_args<Seq>::shell_args(std::string_view const& str) {
        std::optional<std::string> cur_arg;
        auto const append_sv = [&](std::string_view const& part) {
            if (cur_arg)
                cur_arg->append(part);
            else
                cur_arg.emplace(part);
        };
        auto const append_c = [&](char c) {
            if (cur_arg)
                cur_arg->push_back(c);
            else
                cur_arg.emplace(1, c);
        };
        auto const flush = [&]() {
            if (cur_arg) {
                this->push_back(*cur_arg);
                cur_arg.reset();
            }
        };

        std::string_view::size_type dquote_start_pos = std::string_view::npos;
        for (std::string_view::size_type pos = 0; pos < str.length(); ) {
            using namespace std::literals;
            static auto const IFS = " \t\n"sv;
            auto const c = str[pos];

            if (dquote_start_pos == std::string_view::npos &&
                IFS.find(c) != std::string_view::npos) {
                // A whitespace appeared at a non-quoted position. Skip
                // them all.
                flush();
                if (auto const non_ifs_pos = str.find_first_not_of(IFS, pos+1);
                    non_ifs_pos != std::string_view::npos) {

                    pos = non_ifs_pos;
                }
                else {
                    break;
                }
            }
            else if (c == '\'' && dquote_start_pos == std::string_view::npos) {
                // A single quote appeared at a non-quoted
                // position. Find a matching quote.
                if (auto const end_pos = str.find('\'', pos+1);
                    end_pos != std::string_view::npos) {

                    append_sv(str.substr(pos+1, end_pos-pos-1));
                    pos = end_pos+1;
                }
                else {
                    throw unmatched_quoted_string{bad_shell_args{str}, pos};
                }
            }
            else if (c == '"') {
                if (dquote_start_pos == std::string_view::npos) {
                    // A double quote appeared at a non-quoted position.
                    dquote_start_pos = pos;
                    pos++;
                }
                else {
                    // This is the end of a double-quoted string.
                    dquote_start_pos = std::string_view::npos;
                    pos++;
                }
            }
            else if (c == '\\') {
                if (dquote_start_pos == std::string_view::npos) {
                    // A backslash appeared at a non-quoted position. In
                    // this case the backslash always unescapes the next
                    // character, regardless of whether it is special or
                    // not.
                    if (pos+1 < str.length()) {
                        auto const next_c = str[pos+1];
                        if (next_c != '\n') {
                            // But an escaped newline is a special case. It
                            // should be removed from the output.
                            append_c(next_c);
                        }
                        pos += 2;
                    }
                    else {
                        throw orphan_backslash{bad_shell_args{str}, pos};
                    }
                }
                else {
                    // A backslash appeared in a double-quoted string. In
                    // this case the backslash unescapes the next character
                    // only when it's one of certain special
                    // characters. Otherwise it's treated as a literal '\'.
                    if (pos+1 < str.length()) {
                        auto const next_c = str[pos+1];
                        if (next_c == '\n') {
                            // An escaped newline is a special case. It
                            // should be removed from the output.
                            pos += 2;
                        }
                        else if (("$`\"\\"sv).find(next_c) == std::string_view::npos) {
                            // It's not special.
                            append_c(c);
                            pos++;
                        }
                        else {
                            append_c(next_c);
                            pos += 2;
                        }
                    }
                    else {
                        throw orphan_backslash{bad_shell_args{str}, pos};
                    }
                }
            }
            else {
                append_c(c);
                pos++;
            }
        }
        if (dquote_start_pos != std::string_view::npos) {
            throw unmatched_quoted_string{bad_shell_args{str}, dquote_start_pos};
        }
        flush();
    }

    template <template <typename> typename Seq>
    std::string
    shell_args<Seq>::string() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    template <template <typename> typename Seq>
    std::ostream&
    operator<< (std::ostream& out, shell_args<Seq> const& sa) {
        using namespace std::literals;
        static auto const any_specials    = " \t\n~`#$&*()\\|[];'\"<>?"sv;
        static auto const strong_specials = "$`\\\""sv;

        bool is_first = true;
        for (auto const& arg: sa) {
            if (is_first) {
                is_first = false;
            }
            else {
                out << ' ';
            }
            if (arg.find_first_of(any_specials) != shell_args<Seq>::value_type::npos) {
                // The argument contains some shell special
                // characters. Quote it to not confuse someone seeing this
                // message.
                if (arg.find('\'') != shell_args<Seq>::value_type::npos) {
                    // And it contains a single quote, which means
                    // double-quoting is necessary.
                    out << '"';
                    for (typename shell_args<Seq>::value_type::size_type pos = 0; pos < arg.length(); ) {
                        auto const c = arg[pos];
                        if (strong_specials.find(c) == std::string_view::npos) {
                            // This one doesn't need escaping.
                            if (auto const strong_special_pos = arg.find_first_of(strong_specials, pos+1);
                                strong_special_pos != shell_args<Seq>::value_type::npos) {

                                // But there is at least one character that
                                // needs escaping.
                                out << arg.substr(pos, strong_special_pos-pos)
                                    << '\\'
                                    << arg[strong_special_pos];
                                pos = strong_special_pos+1;
                            }
                            else {
                                // Nothing needs escaping.
                                out << arg.substr(pos);
                                break;
                            }
                        }
                        else {
                            // This one needs escaping.
                            out << '\\' << c;
                        }
                    }
                    out << '"';
                }
                else {
                    // It contains no single quotes, which means we can
                    // single-quote the argument.
                    out << '\'' << arg << '\'';
                }
            }
            else {
                // It contains no special characters. No quoting is
                // required.
                out << arg;
            }
        }
        return out;
    }
}
