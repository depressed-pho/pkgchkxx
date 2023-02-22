#include <cerrno>

#include "wwwstream.hxx"

namespace pkg_chk {
    wwwstreambuf::wwwstreambuf(std::string const& url) {
        if (fetchIO* fio = fetchGetURL(url.c_str(), ""); fio != nullptr) {
            _fio = std::unique_ptr<fetchIO, fetchIO_deleter>(fio);
        }
        else {
            // This is obviously thread-unsafe. We may have to change the
            // backend to libcurl.
            switch (fetchLastErrCode) {
                case FETCH_UNAVAIL:
                    throw remote_file_unavailable("file not available: " + url);

                default:
                    throw remote_file_error(fetchLastErrString);
            }
        }
    }

    wwwstreambuf::int_type
    wwwstreambuf::underflow() {
        if (eback() == nullptr) {
            // An underflow has happened because we haven't allocated a
            // buffer yet.
            _read_buf = buffer_t();
        }

        while (true) {
            ssize_t const n_read = fetchIO_read(_fio.get(), _read_buf->data(), _read_buf->size());
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

    wwwstreambuf::int_type
    wwwstreambuf::pbackfail(int_type ch) {
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
