#include <algorithm>
#include <cassert>

#include "gzipstream.hxx"

namespace pkgxx {
    gzipstreambuf::gzipstreambuf(std::streambuf* const base, int const level)
        : _base{base}
        , _deflate_in {std::make_unique<buffer_t>()}
        , _deflate_out{std::make_unique<buffer_t>()} {

        _deflate.avail_in = 0;
        _deflate.zalloc   = nullptr;
        _deflate.zfree    = nullptr;
        _deflate.opaque   = nullptr;

        if (deflateInit2(&_deflate, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            throw std::runtime_error{_deflate.msg};
        }
    }

    gzipstreambuf::~gzipstreambuf() {
        // We might, or might not have anything more to compress, but zlib
        // can have some pending output regardless of that.
        _deflate.next_in  = reinterpret_cast<Bytef*>(_deflate_in->data());
        _deflate.avail_in = static_cast<uInt>(pptr() - pbase());
        while (true) {
            _deflate.next_out  = reinterpret_cast<Bytef*>(_deflate_out->data());
            _deflate.avail_out = static_cast<uInt>(_deflate_out->size());
            int const rv = deflate(&_deflate, Z_FINISH);
            if (rv == Z_STREAM_END || rv == Z_OK) {
                if (_deflate.avail_out < _deflate_out->size()) {
                    // Got some compressed data from zlib.
                    auto const n_gzipped = _deflate_out->size() - _deflate.avail_out;
                    for (std::size_t n_written = 0; n_written < n_gzipped; ) {
                        std::streamsize const n_put =
                            _base->sputn(
                                _deflate_out->data() + n_written,
                                static_cast<std::streamsize>(n_gzipped - n_written));
                        if (n_put > 0) {
                            // We have successfully written some data to
                            // the underlying buffer.
                            n_written += static_cast<std::size_t>(n_put);
                            continue;
                        }
                        else {
                            // The underlying buffer is now closed.
                            break;
                        }
                    }
                    // Now we have sent all the available compressed data
                    // to the underlying buffer.
                }
                if (rv == Z_STREAM_END) {
                    // Zlib said it had nothing more to output. Great!
                    break;
                }
            }
            else {
                // Even Z_BUF_ERROR is a fatal error, because we told zlib
                // that it was the last chunk of data to feed it, and there
                // was definitely some output space for it also. Zlib
                // saying "no progress could be made" makes no sense.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wterminate"  // GCC
#pragma GCC diagnostic ignored "-Wexceptions" // Clang
                // We know this is bad, but zlib failing its job is bad
                // enough to abort the process. The destructor cannot be
                // marked as noexcept(false) because we have inherited it
                // from the base class.
                throw std::runtime_error{_deflate.msg};
#pragma GCC diagnostic pop
            }
        }

        int const rv = deflateEnd(&_deflate);
        assert(rv == Z_OK);
    }

#if !defined(DOXYGEN)
    gzipstreambuf::int_type
    gzipstreambuf::overflow(int_type ch) {
        _deflate.next_in  = reinterpret_cast<Bytef*>(_deflate_in->data());
        _deflate.avail_in = static_cast<uInt>(pptr() - pbase());
        int flush_mode    = Z_NO_FLUSH;
        while (true) {
            _deflate.next_out  = reinterpret_cast<Bytef*>(_deflate_out->data());
            _deflate.avail_out = static_cast<uInt>(_deflate_out->size());
            int const rv       = deflate(&_deflate, flush_mode);
            if (rv == Z_OK) {
                if (_deflate.avail_out < _deflate_out->size()) {
                    // Got some compressed data from zlib.
                    auto const n_gzipped = _deflate_out->size() - _deflate.avail_out;
                    for (std::size_t n_written = 0; n_written < n_gzipped; ) {
                        std::streamsize const n_put =
                            _base->sputn(
                                _deflate_out->data() + n_written,
                                static_cast<std::streamsize>(n_gzipped - n_written));
                        if (n_put > 0) {
                            // We have successfully written some data to
                            // the underlying buffer.
                            n_written += static_cast<std::size_t>(n_put);
                            continue;
                        }
                        else {
                            // The underlying buffer is now closed.
                            return traits_type::eof();
                        }
                    }
                    // Now we have sent all the available compressed data
                    // to the underlying buffer.
                }
                if (_deflate.avail_in > 0) {
                    // But zlib could not consume all the data we fed it,
                    // presumably because there was not enough space for
                    // the output. Try again.
                    continue;
                }
                // Now zlib consumed everything, and we took everything
                // from its buffer.
                setp(_deflate_in->data(),
                     _deflate_in->data() + _deflate_in->size());
                break;
            }
            else if (rv == Z_BUF_ERROR && flush_mode == Z_NO_FLUSH) {
                // Zlib made no progress. Since we definitely had space for
                // its output, this means zlib wanted more input to
                // efficiently compress data.
                if (_deflate.avail_in + 1 < _deflate_in->size()) {
                    // Okay then. As long as we have at least two more
                    // characters to write to our stream, we can return
                    // from overflow(). But before that we must move the
                    // leftover in the buffer to the front.
                    std::move(
                        _deflate.next_in,
                        _deflate.next_in + _deflate.avail_in,
                        reinterpret_cast<Bytef*>(_deflate_out->data()));
                    setp(_deflate_in->data() + _deflate.avail_in,
                         _deflate_in->data() + _deflate_in->size());
                    break;
                }
                else {
                    // Our buffer is full but zlib still insists on getting
                    // more data. This is unacceptable. Have it flush
                    // forcefully.
                    flush_mode = Z_SYNC_FLUSH;
                    continue;
                }
            }
            else {
                throw std::runtime_error{_deflate.msg};
            }
        }

        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            char_type const c = traits_type::to_char_type(ch);
            *pptr() = c;
            pbump(1);
        }

        return ch;
    }
#endif

    gunzipstreambuf::gunzipstreambuf(std::streambuf* base)
        : _base(base)
        , _inflate_eof {false}
        , _inflate_done{false}
        , _inflate_in  {std::make_unique<buffer_t>()}
        , _inflate_out {std::make_unique<buffer_t>()} {

        _inflate.avail_in = 0;
        _inflate.zalloc   = nullptr;
        _inflate.zfree    = nullptr;
        _inflate.opaque   = nullptr;

        if (inflateInit2(&_inflate, 15 + 32) != Z_OK) {
            throw std::runtime_error(_inflate.msg);
        }
    }

    gunzipstreambuf::~gunzipstreambuf() {
        int const rv = inflateEnd(&_inflate);
        assert(rv == Z_OK);
    }

#if !defined(DOXYGEN)
    gunzipstreambuf::int_type
    gunzipstreambuf::underflow() {
        while (!_inflate_done) {
            if (_inflate.avail_in == 0 && !_inflate_eof) {
                // zlib has no unconsumed compressed input, and the base
                // streambuf hasn't got EOF yet. Try reading some.
                _inflate.next_in  = reinterpret_cast<Bytef*>(_inflate_in->data());
                _inflate.avail_in = 0;

                if (auto const avail = _base->in_avail(); avail > 0) {
                    std::streamsize const n_read = _base->sgetn(
                        _inflate_in->data(),
                        static_cast<std::streamsize>(_inflate_in->size()));
                    _inflate.avail_in = static_cast<uInt>(n_read);
                }
                else {
                    // The base streambuf can't provide us any data without
                    // blocking. Allow it to block.
                    int_type const ch = _base->sbumpc();
                    if (traits_type::eq_int_type(ch, traits_type::eof())) {
                        // But it got EOF.
                        _inflate_eof = true;
                    }
                    else {
                        (*_inflate_in)[0] = traits_type::to_char_type(ch);
                        _inflate.avail_in = 1;
                    }
                }
            }

            _inflate.next_out  = reinterpret_cast<Bytef*>(_inflate_out->data());
            _inflate.avail_out = static_cast<uInt>(_inflate_out->size());
            switch (inflate(&_inflate, Z_SYNC_FLUSH)) {
            case Z_OK:
                if (_inflate.avail_out < _inflate_out->size()) {
                    // Got some uncompressed data from zlib.
                    auto const n_gunzipped = _inflate_out->size() - _inflate.avail_out;
                    setg(_inflate_out->data(),
                         _inflate_out->data(),
                         _inflate_out->data() + n_gunzipped);
                    return traits_type::to_int_type(*gptr());
                }
                else {
                    // zlib needs more input to produce a single output
                    // byte.
                    continue;
                }

            case Z_STREAM_END:
                // Getting Z_STREAM_END means that we have provided no
                // input data to zlib because the base streambuf has
                // already got EOF.
                _inflate_done = true;
                // But zlib may have produced the last chunk of
                // uncompressed data.
                if (_inflate.avail_out < _inflate_out->size()) {
                    auto const n_gunzipped = _inflate_out->size() - _inflate.avail_out;
                    setg(_inflate_out->data(),
                         _inflate_out->data(),
                         _inflate_out->data() + n_gunzipped);
                    return traits_type::to_int_type(*gptr());
                }
                else {
                    // No it didn't.
                    return traits_type::eof();
                }

            default:
                throw std::runtime_error{_inflate.msg};
            }
        }

        return traits_type::eof();
    }
#endif

#if !defined(DOXYGEN)
    gunzipstreambuf::int_type
    gunzipstreambuf::pbackfail(int_type ch) {
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
#endif
}
