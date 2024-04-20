#include "gzipstream.hxx"

namespace pkgxx {
    gunzipstreambuf::gunzipstreambuf(std::streambuf* base)
        : _base(base)
        , _inflate_eof(false)
        , _inflate_done(false) {

        _inflate.next_in  = nullptr;
        _inflate.avail_in = 0;
        _inflate.zalloc   = nullptr;
        _inflate.zfree    = nullptr;
        _inflate.opaque   = nullptr;
        if (inflateInit2(&_inflate, 15 + 32) != Z_OK) {
            throw std::runtime_error(_inflate.msg);
        }
    }

    gunzipstreambuf::~gunzipstreambuf() {
        inflateEnd(&_inflate);
    }

#if !defined(DOXYGEN)
    gunzipstreambuf::int_type
    gunzipstreambuf::underflow() {
        if (eback() == nullptr) {
            // An underflow has happened because we haven't allocated
            // buffers yet.
            _inflate_in  = buffer_t();
            _inflate_out = buffer_t();
        }

        while (!_inflate_done) {
            if (_inflate.avail_in == 0 && !_inflate_eof) {
                // zlib has no unconsumed compressed input, and the base
                // streambuf hasn't got EOF yet. Try reading some.
                _inflate.next_in  = reinterpret_cast<Bytef*>(_inflate_in->data());
                _inflate.avail_in = 0;

                if (auto avail = _base->in_avail(); avail > 0) {
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
                    auto const n_read = _inflate_out->size() - _inflate.avail_out;
                    setg(_inflate_out->data(),
                         _inflate_out->data(),
                         _inflate_out->data() + n_read);
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
                    auto const n_read = _inflate_out->size() - _inflate.avail_out;
                    setg(_inflate_out->data(),
                         _inflate_out->data(),
                         _inflate_out->data() + n_read);
                    return traits_type::to_int_type(*gptr());
                }
                else {
                    // No it didn't.
                    return traits_type::eof();
                }

            default:
                throw std::runtime_error(_inflate.msg);
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
