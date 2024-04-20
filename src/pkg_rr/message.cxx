#include <algorithm>

#include "message.hxx"

namespace pkg_rr {
    void
    msg_logger::msg_buf::print_prefix() {
        switch (_state) {
        case state::initial:
            _out << "RR> ";
            break;
        case state::newline:
            _out << "rr> ";
            break;
        default:
            break;
        }
    }

    msg_logger::msg_buf::int_type
    msg_logger::msg_buf::overflow(int_type ch) {
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            char_type const c = traits_type::to_char_type(ch);

            print_prefix();
            _out.put(c);

            _state = (c == '\n') ? state::newline : state::general;
        }
        return ch;
    }

    std::streamsize
    msg_logger::msg_buf::xsputn(const char_type* s, std::streamsize count) {
        auto const s_end = s + count;
        auto l_begin = s;
        while (true) {
            auto l_end = std::find(l_begin, s_end, '\n');

            print_prefix();

            _out.write(l_begin, s_end - l_begin);

            if (l_end == s_end) {
                _state = state::general;
                break;
            }
            else {
                _state = state::newline;
            }
        }
        return count;
    }
}
