#include <cerrno>
#include <unistd.h>

#include "fdstream.hxx"

namespace pkg_chk {
    fdstreambuf::fdstreambuf(int fd)
        : _fd(fd) {}

    fdstreambuf::~fdstreambuf() {
        close();
    }

    fdstreambuf*
    fdstreambuf::close() {
        if (_fd >= 0) {
            overflow(traits_type::eof());
            ::close(_fd);
            _fd = -1;
        }
        return this;
    }

    fdstreambuf::int_type
    fdstreambuf::overflow(int_type ch) {
        if (pbase() == nullptr) {
            // An overflow has happened because we haven't allocated a
            // buffer yet.
            _write_buf = buffer_t();
        }
        else if (pptr() > pbase()) {
            // An overflow has happened either because the buffer became
            // full, or because it's being closed. Flush it now.
            size_t const n_write = pptr() - pbase();
            for (size_t n_remaining = n_write; n_remaining > 0; ) {
                ssize_t const n_written = write(_fd, _write_buf->data(), n_write);
                if (n_written > 0) {
                    n_remaining -= n_written;
                    continue;
                }
                else if (n_written == -1 && errno == EINTR) {
                    continue;
                }
                else {
                    return traits_type::eof();
                }
            }
        }
        setp(_write_buf->data(),
             _write_buf->data() + _write_buf->size());

        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            char_type const c = traits_type::to_char_type(ch);
            *pptr() = c;
            pbump(1);
        }

        return ch;
    }

    fdstreambuf::int_type
    fdstreambuf::underflow() {
        if (eback() == nullptr) {
            // An underflow has happened because we haven't allocated a
            // buffer yet.
            _read_buf = buffer_t();
        }

        while (true) {
            ssize_t const n_read = read(_fd, _read_buf->data(), _read_buf->size());
            if (n_read > 0) {
                setg(_read_buf->data(),
                     _read_buf->data(),
                     _read_buf->data() + n_read);
                break;
            }
            else if (n_read == -1 && errno == EINTR) {
                continue;
            }
            else {
                return traits_type::eof();
            }
        }

        return traits_type::to_int_type(*gptr());
    }

    fdstreambuf::int_type
    fdstreambuf::pbackfail(int_type ch) {
        if (!traits_type::eq_int_type(ch, traits_type::eof()) &&
            gptr() != nullptr &&
            gptr() > eback()) {

            // There is no problem modifying the buffer.
            gptr()[-1] = traits_type::to_char_type(ch);
            return ch;
        }
        else {
            // We don't support putting back characters past the
            // limit. That would complicate the implementation.
            return traits_type::eof();
        }
    }
}
