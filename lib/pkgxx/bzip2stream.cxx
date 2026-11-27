#include <cassert>
#include <sstream>

#include "bzip2stream.hxx"

namespace pkgxx {
    bzip2_error::bzip2_error(int code)
        : std::runtime_error{message(code)} {}

    std::string
    bzip2_error::message(int code) {
        switch (code) {
        case BZ_SEQUENCE_ERROR:
            return "BZ_SEQUENCE_ERROR";
        case BZ_PARAM_ERROR:
            return "BZ_PARAM_ERROR";
        case BZ_MEM_ERROR:
            return "BZ_MEM_ERROR";
        case BZ_DATA_ERROR:
            return "BZ_DATA_ERROR";
        case BZ_DATA_ERROR_MAGIC:
            return "BZ_DATA_ERROR_MAGIC";
        case BZ_IO_ERROR:
            return "BZ_IO_ERROR";
        case BZ_UNEXPECTED_EOF:
            return "BZ_UNEXPECTED_EOF";
        case BZ_OUTBUFF_FULL:
            return "BZ_OUTBUFF_FULL";
        case BZ_CONFIG_ERROR:
            return "BZ_CONFIG_ERROR";
        default:
            std::stringstream s;
            s << "Unknown bzip2 error: " << code;
            return s.str();
        }
    }

    bzip2streambuf::bzip2streambuf(std::streambuf* const base, int const block_size)
        : _base     {base}
        , _bzip2_in {std::make_unique<buffer_t>()}
        , _bzip2_out{std::make_unique<buffer_t>()} {

        _bzip2.avail_in = 0;
        _bzip2.bzalloc  = nullptr;
        _bzip2.bzfree   = nullptr;
        _bzip2.opaque   = nullptr;

        if (auto const rv = BZ2_bzCompressInit(&_bzip2, block_size, 0, 0); rv != BZ_OK) {
            throw bzip2_error{rv};
        }
    }

    bzip2streambuf::~bzip2streambuf() {
        // We might, or might not have anything more to compress, but bzip2
        // can have some pending output regardless of that.
        _bzip2.next_in  = _bzip2_in->data();
        _bzip2.avail_in = static_cast<unsigned>(pptr() - pbase());
        while (true) {
            _bzip2.next_out  = _bzip2_out->data();
            _bzip2.avail_out = static_cast<unsigned>(_bzip2_out->size());
            int const rv     = BZ2_bzCompress(&_bzip2, BZ_FINISH);
            if (rv == BZ_STREAM_END || rv == BZ_FINISH_OK) {
                if (_bzip2.avail_out < _bzip2_out->size()) {
                    // Got some compressed data from bzip2.
                    auto const n_bzipped = _bzip2_out->size() - _bzip2.avail_out;
                    for (std::size_t n_written = 0; n_written < n_bzipped; ) {
                        std::streamsize const n_put =
                            _base->sputn(
                                _bzip2_out->data() + n_written,
                                static_cast<std::streamsize>(n_bzipped - n_written));
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
                if (rv == BZ_STREAM_END) {
                    // Bzip2 said it had nothing more to output. Great!
                    break;
                }
            }
            else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wterminate"  // GCC
#pragma GCC diagnostic ignored "-Wexceptions" // Clang
                // We know this is bad, but bzip2 failing its job is bad
                // enough to abort the process. The destructor cannot be
                // marked as noexcept(false) because we have inherited it
                // from the base class.
                throw bzip2_error{rv};
            }
        }

        int const rv = BZ2_bzCompressEnd(&_bzip2);
        assert(rv == BZ_OK);
    }

#if !defined(DOXYGEN)
    bzip2streambuf::int_type
    bzip2streambuf::overflow(int_type ch) {
        if (auto const to_compress = pptr() - pbase(); to_compress > 0) {
            _bzip2.next_in  = _bzip2_in->data();
            _bzip2.avail_in = static_cast<unsigned>(to_compress);
            int action      = BZ_RUN;
            while (true) {
                _bzip2.next_out  = _bzip2_out->data();
                _bzip2.avail_out = static_cast<unsigned>(_bzip2_out->size());
                int const rv     = BZ2_bzCompress(&_bzip2, action);
                if (rv == BZ_RUN_OK || rv == BZ_FLUSH_OK) {
                    if (_bzip2.avail_out < _bzip2_out->size()) {
                        // Got some compressed data from bzip2.
                        auto const n_bzipped = _bzip2_out->size() - _bzip2.avail_out;
                        for (std::size_t n_written = 0; n_written < n_bzipped; ) {
                            std::streamsize const n_put =
                                _base->sputn(
                                    _bzip2_out->data() + n_written,
                                    static_cast<std::streamsize>(n_bzipped - n_written));
                            if (n_put > 0) {
                                // We have successfully written some data
                                // to the underlying buffer.
                                n_written += static_cast<std::size_t>(n_put);
                                continue;
                            }
                            else {
                                // The underlying buffer is now closed.
                                return traits_type::eof();
                            }
                        }
                        // Now we have sent all the available compressed
                        // data to the underlying buffer.
                        if (_bzip2.avail_in > 0) {
                            // But bzip2 did not consume all the data we
                            // fed it, presumably because there was not
                            // enough space for the output. Try again.
                            continue;
                        }
                        else {
                            // Now bzip2 consumed everything, and we took
                            // everything from its buffer.
                            setp(_bzip2_in->data(),
                                 _bzip2_in->data() + _bzip2_in->size());
                            break;
                        }
                    }
                    else {
                        // Got no compressed data from bzip2.
                        if (_bzip2.avail_in + 1 < _bzip2_in->size()) {
                            // Okay then. As long as we have at least two
                            // more characters to write to our stream, we
                            // can return from overflow(). But before that
                            // we must move the leftover in the buffer to
                            // the front.
                            std::move(
                                _bzip2.next_in,
                                _bzip2.next_in + _bzip2.avail_in,
                                _bzip2_out->data());
                            setp(_bzip2_in->data() + _bzip2.avail_in,
                                 _bzip2_in->data() + _bzip2_in->size());
                            break;
                        }
                        else {
                            // Our buffer is full but bzip2 still insists
                            // on getting more data. This is
                            // unacceptable. Have it flush forcefully.
                            action = BZ_FLUSH;
                            continue;
                        }
                    }
                }
                else {
                    throw bzip2_error{rv};
                }
            }
        }
        else {
            // We had nothing to compress. BZ2_bzCompress(BZ_RUN) returns
            // BZ_PARAM_ERROR when it is supplied no input, so don't even
            // try. This is probably the first call of overflow() after
            // construction.
            setp(_bzip2_in->data() + _bzip2.avail_in,
                 _bzip2_in->data() + _bzip2_in->size());
        }

        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            char_type const c = traits_type::to_char_type(ch);
            *pptr() = c;
            pbump(1);
        }

        return ch;
    }
#endif

    bunzip2streambuf::bunzip2streambuf(std::streambuf* const base)
        : _base        {base}
        , _bunzip2_eof {false}
        , _bunzip2_done{false}
        , _bunzip2_in  {std::make_unique<buffer_t>()}
        , _bunzip2_out {std::make_unique<buffer_t>()} {

        _bunzip2.avail_in = 0;
        _bunzip2.bzalloc  = nullptr;
        _bunzip2.bzfree   = nullptr;
        _bunzip2.opaque   = nullptr;

        if (auto const rv = BZ2_bzDecompressInit(&_bunzip2, 0, 0); rv != BZ_OK) {
            throw bzip2_error{rv};
        }
    }

    bunzip2streambuf::~bunzip2streambuf() {
        int const rv = BZ2_bzDecompressEnd(&_bunzip2);
        assert(rv == BZ_OK);
    }

#if !defined(DOXYGEN)
    bunzip2streambuf::int_type
    bunzip2streambuf::underflow() {
        while (!_bunzip2_done) {
            if (_bunzip2.avail_in == 0 && !_bunzip2_eof) {
                // bzip2 has no unconsumed compressed input, and the base
                // streambuf hasn't got EOF yet. Try reading some.
                _bunzip2.next_in  = _bunzip2_in->data();
                _bunzip2.avail_in = 0;

                if (auto avail = _base->in_avail(); avail > 0) {
                    std::streamsize const n_read = _base->sgetn(
                        _bunzip2_in->data(),
                        static_cast<std::streamsize>(_bunzip2_in->size()));
                    _bunzip2.avail_in = static_cast<unsigned>(n_read);
                }
                else {
                    // The base streambuf can't provide us any data without
                    // blocking. Allow it to block.
                    int_type const ch = _base->sbumpc();
                    if (traits_type::eq_int_type(ch, traits_type::eof())) {
                        // But it got EOF.
                        _bunzip2_eof = true;
                    }
                    else {
                        (*_bunzip2_in)[0] = traits_type::to_char_type(ch);
                        _bunzip2.avail_in = 1;
                    }
                }
            }

            _bunzip2.next_out  = _bunzip2_out->data();
            _bunzip2.avail_out = static_cast<unsigned>(_bunzip2_out->size());
            auto const rv = BZ2_bzDecompress(&_bunzip2);
            switch (rv) {
            case BZ_OK:
                if (_bunzip2.avail_out < _bunzip2_out->size()) {
                    // Got some uncompressed data from bzip2.
                    auto const n_bunzipped = _bunzip2_out->size() - _bunzip2.avail_out;
                    setg(_bunzip2_out->data(),
                         _bunzip2_out->data(),
                         _bunzip2_out->data() + n_bunzipped);
                    return traits_type::to_int_type(*gptr());
                }
                else {
                    // bzip2 needs more input to produce a single output
                    // byte.
                    continue;
                }

            case BZ_STREAM_END:
                // Getting BZ_STREAM_END means that we have reached the
                // logical end of compressed bzip2 data.
                _bunzip2_done = true;
                // But bzip2 may have produced the last chunk of
                // uncompressed data.
                if (_bunzip2.avail_out < _bunzip2_out->size()) {
                    auto const n_bunzipped = _bunzip2_out->size() - _bunzip2.avail_out;
                    setg(_bunzip2_out->data(),
                         _bunzip2_out->data(),
                         _bunzip2_out->data() + n_bunzipped);
                    return traits_type::to_int_type(*gptr());
                }
                else {
                    // No it didn't.
                    return traits_type::eof();
                }

            default:
                throw bzip2_error{rv};
            }
        }

        return traits_type::eof();
    }
#endif

#if !defined(DOXYGEN)
    bunzip2streambuf::int_type
    bunzip2streambuf::pbackfail(int_type ch) {
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
