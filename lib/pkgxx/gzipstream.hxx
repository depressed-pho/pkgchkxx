#pragma once

#include <array>
#include <istream>
#include <memory>
#include <optional>
#include <streambuf>
#include <zlib.h>

namespace pkgxx {
    /** A stream buffer that works with gzipped data. Currently only
     * supports reading operations.
     */
    struct gunzipstreambuf: public std::streambuf {
        /** Construct a stream buffer reading gzipped data from another
         * stream buffer. */
        gunzipstreambuf(std::streambuf* base);
        virtual ~gunzipstreambuf();

    protected:
#if !defined(DOXYGEN)
        virtual int_type
        underflow() override;

        virtual int_type
        pbackfail(int_type ch = traits_type::eof()) override;
#endif

    private:
        static constexpr int const buf_size = 1024;
        using buffer_t = std::array<char_type, buf_size>;

        std::streambuf* _base;

        z_stream_s _inflate;
        bool _inflate_eof;  // Got EOF from _base.
        bool _inflate_done; // Got Z_STREAM_END from inflate().
        std::optional<buffer_t> _inflate_in;
        std::optional<buffer_t> _inflate_out;
    };

    /** An input stream that reads gzipped data.
     */
    struct gunzipistream: public std::istream {
        /** Construct an input stream that reads gzip-compressed data from
         * an another istream.
         */
        gunzipistream(std::istream& base)
            : std::istream(nullptr) {

            if (auto* base_buf = base.rdbuf(); base_buf != nullptr) {
                _buf = std::make_unique<gunzipstreambuf>(base_buf);
                rdbuf(_buf.get());
            }
        }

        /** Construct an instance of \ref gunzipistream by moving a buffer
         * out of another instance. */
        gunzipistream(gunzipistream&& other)
            : std::istream(std::move(other))
            , _buf(std::move(other._buf)) {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

    private:
        std::unique_ptr<gunzipstreambuf> _buf;
    };
}
