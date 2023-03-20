#include <exception>
#include <sstream>

#include "bzip2stream.hxx"

namespace {
    std::runtime_error
    bz2_exception(int code) {
        switch (code) {
        case BZ_SEQUENCE_ERROR:
            return std::runtime_error("BZ_SEQUENCE_ERROR");
        case BZ_PARAM_ERROR:
            return std::runtime_error("BZ_PARAM_ERROR");
        case BZ_MEM_ERROR:
            return std::runtime_error("BZ_MEM_ERROR");
        case BZ_DATA_ERROR:
            return std::runtime_error("BZ_DATA_ERROR");
        case BZ_DATA_ERROR_MAGIC:
            return std::runtime_error("BZ_DATA_ERROR_MAGIC");
        case BZ_IO_ERROR:
            return std::runtime_error("BZ_IO_ERROR");
        case BZ_UNEXPECTED_EOF:
            return std::runtime_error("BZ_UNEXPECTED_EOF");
        case BZ_OUTBUFF_FULL:
            return std::runtime_error("BZ_OUTBUFF_FULL");
        case BZ_CONFIG_ERROR:
            return std::runtime_error("BZ_CONFIG_ERROR");
        default:
            std::stringstream s;
            s << "Unknown bzip2 error: " << code;
            return std::runtime_error(s.str());
        }
    }
}

namespace pkgxx {
    bunzip2streambuf::bunzip2streambuf(std::streambuf* base)
        : _base(base)
        , _bunzip2_eof(false)
        , _bunzip2_done(false) {

        _bunzip2.avail_in = 0;
        _bunzip2.bzalloc  = nullptr;
        _bunzip2.bzfree   = nullptr;
        _bunzip2.opaque   = nullptr;
        if (auto const res = BZ2_bzDecompressInit(&_bunzip2, 0, 0); res != BZ_OK) {
            throw bz2_exception(res);
        }
    }

    bunzip2streambuf::~bunzip2streambuf() {
        BZ2_bzDecompressEnd(&_bunzip2);
    }

    bunzip2streambuf::int_type
    bunzip2streambuf::underflow() {
        if (eback() == nullptr) {
            // An underflow has happened because we haven't allocated
            // buffers yet.
            _bunzip2_in  = buffer_t();
            _bunzip2_out = buffer_t();
        }

        while (!_bunzip2_done) {
            if (_bunzip2.avail_in == 0 && !_bunzip2_eof) {
                // zlib has no unconsumed compressed input, and the base
                // streambuf hasn't got EOF yet. Try reading some.
                _bunzip2.next_in  = _bunzip2_in->data();
                _bunzip2.avail_in = 0;

                if (auto avail = _base->in_avail(); avail > 0) {
                    auto const n_read = _base->sgetn(_bunzip2_in->data(), _bunzip2_in->size());
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
            auto const res = BZ2_bzDecompress(&_bunzip2);
            switch (res) {
            case BZ_OK:
                if (_bunzip2.avail_out < _bunzip2_out->size()) {
                    // Got some uncompressed data from bzip2.
                    auto const n_read = _bunzip2_out->size() - _bunzip2.avail_out;
                    setg(_bunzip2_out->data(),
                         _bunzip2_out->data(),
                         _bunzip2_out->data() + n_read);
                    return traits_type::to_int_type(*gptr());
                }
                else {
                    // bzip2 needs more input to produce a single output
                    // byte.
                    continue;
                }

            case BZ_STREAM_END:
                // Getting Z_STREAM_END means that we have reached the
                // logical end of compressed bzip2 data.
                _bunzip2_done = true;
                // But bzip2 may have produced the last chunk of
                // uncompressed data.
                if (_bunzip2.avail_out < _bunzip2_out->size()) {
                    auto const n_read = _bunzip2_out->size() - _bunzip2.avail_out;
                    setg(_bunzip2_out->data(),
                         _bunzip2_out->data(),
                         _bunzip2_out->data() + n_read);
                    return traits_type::to_int_type(*gptr());
                }
                else {
                    // No it didn't.
                    return traits_type::eof();
                }

            default:
                throw bz2_exception(res);
            }
        }

        return traits_type::eof();
    }

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
}
