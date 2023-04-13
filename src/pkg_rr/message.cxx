#include <algorithm>

#include "message.hxx"

namespace pkg_rr {
    msg_logger::msg_buf::int_type
    msg_logger::msg_buf::overflow(int_type ch) {
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            char_type const c = traits_type::to_char_type(ch);

            if (_cont) {
                _out << "rr> ";
                _cont = false;
            }

            _out.put(c);

            if (c == '\n') {
                _cont = true;
            }
        }
        return ch;
    }

    std::streamsize
    msg_logger::msg_buf::xsputn(const char_type* s, std::streamsize count) {
        auto const s_end = s + count;
        auto l_begin = s;
        while (true) {
            auto l_end = std::find(l_begin, s_end, '\n');

            if (_cont) {
                _out << "rr> ";
                _cont = false;
            }

            _out.write(l_begin, s_end - l_begin);

            if (l_end == s_end) {
                break;
            }
            else {
                _cont = true;
            }
        }
        return count;
    }
}
