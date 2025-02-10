#include <algorithm>
#include <cstring>

#include "message.hxx"

namespace pkg_rr {
    msgstreambuf::msgstreambuf(
        pkgxx::ttystreambuf_base& out,
        pkgxx::tty::style const& default_style)
        : _out(out)
        , _state(state::initial)
        , _default_sty(default_style)
        , _RR_sty()
        , _rr_sty() {

        _out.push_style(_default_sty, pkgxx::ttystreambuf_base::how::no_combine);
    }

    msgstreambuf::~msgstreambuf() {
        _out.pop_style();
        _out.pubsync();
        // It is extremely important to flush the tty here because we
        // aren't the only process that writes to this tty. The other
        // processes may also write to it, and if we didn't flush the SGR
        // reset in our buffer their output would be coloured.
    }

    [[noreturn]] std::lock_guard<std::mutex>
    msgstreambuf::lock() {
        assert(0 && "It's a logical error to lock a msgstreambuf");
        std::terminate();
    }

    std::optional<pkgxx::dimension<std::size_t>>
    msgstreambuf::term_size() const {
        return _out.term_size();
    }

    void
    msgstreambuf::push_style(
        pkgxx::tty::style const& sty,
        pkgxx::ttystream_base::how how_) {

        _out.push_style(sty, how_);
    }

    void
    msgstreambuf::pop_style() {
        _out.pop_style();
    }

    int
    msgstreambuf::sync() {
        return _out.pubsync();
    }

    msgstreambuf::int_type
    msgstreambuf::overflow(int_type ch) {
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            char_type const c = traits_type::to_char_type(ch);

            print_prefix();
            auto ret = _out.sputc(c);

            _state = (c == '\n') ? state::newline : state::general;
            return ret;
        }
        else {
            return ch;
        }
    }

    std::streamsize
    msgstreambuf::xsputn(const char_type* s, std::streamsize count) {
        auto const s_end = s + count;
        auto l_begin = s;
        while (true) {
            auto l_end = std::find(l_begin, s_end, '\n');

            print_prefix();
            _out.sputn(l_begin, l_end - l_begin);

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

    void
    msgstreambuf::print_prefix() {
        switch (_state) {
        case state::initial:
            _out.push_style(
                _RR_sty + _default_sty,
                pkgxx::ttystreambuf_base::how::no_combine);
            _out.sputn("RR> ", std::strlen("RR> "));
            _out.pop_style();
            break;

        case state::newline:
            _out.push_style(
                _rr_sty + _default_sty,
                pkgxx::ttystreambuf_base::how::no_combine);
            _out.sputn("rr> ", std::strlen("rr> "));
            _out.pop_style();
            break;

        default:
            break;
        }
    }

    msgstream::msgstream()
        : std::ostream(nullptr)
        , _buf(nullptr) {}

    msgstream::msgstream(ttystream_base& out, pkgxx::tty::style const& default_style)
        : std::ostream(nullptr)
        , _buf(out.rdbuf()
               ? std::make_unique<msgstreambuf>(
                   dynamic_cast<pkgxx::ttystreambuf_base&>(*out.rdbuf()), default_style)
               : nullptr) {

        rdbuf(_buf.get());
    }

    msgstream::msgstream(msgstream&& other)
        : std::ostream(nullptr)
        , _buf(std::move(other._buf)) {

        other.set_rdbuf(nullptr);
        rdbuf(_buf.get());
    }

    std::optional<pkgxx::dimension<std::size_t>>
    msgstream::size() const {
        return _buf ? _buf->term_size() : std::nullopt;
    }

    void
    msgstream::push_style(pkgxx::tty::style const& sty, how how_) {
        if (_buf) {
            _buf->push_style(sty, how_);
        }
    }

    void
    msgstream::pop_style() {
        if (_buf) {
            _buf->pop_style();
        }
    }
}
