#pragma once

#include <array>
#include <istream>
#include <memory>
#include <streambuf>
#include <zlib.h>

namespace pkgxx {
    /** A stream buffer that compresses data with gzip. Currently only
     * supports write operations. That is, one can write uncompressed data
     * to this buffer and compress it, but one cannot read uncompressed
     * data from another buffer to obtain compressed data from this buffer.
     */
    struct gzipstreambuf: public std::streambuf {
        /** Construct a stream buffer writing gzipped data to another
         * stream buffer.
         */
        gzipstreambuf(
            /// The underlying buffer.
            std::streambuf* base,
            /// The compression level must be in the range of [0, 9].
            int level = Z_DEFAULT_COMPRESSION);

        virtual ~gzipstreambuf();

    protected:
#if !defined(DOXYGEN)
        virtual int_type
        overflow(int_type ch = traits_type::eof()) override;
#endif

    private:
        static constexpr int const buf_size = 65536;
        using buffer_t = std::array<char_type, buf_size>;

        std::streambuf*           _base;
        z_stream_s                _deflate;
        std::unique_ptr<buffer_t> _deflate_in;
        std::unique_ptr<buffer_t> _deflate_out;
    };

    /** A stream buffer that decompresses gzipped data. Currently only
     * supports read operations. That is, one can read compressed data from
     * another buffer and decompress it, but one cannot write compressed
     * data to this buffer and transfer decompressed data to another
     * buffer.
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
        static constexpr int const buf_size = 65536;
        using buffer_t = std::array<char_type, buf_size>;

        std::streambuf*           _base;
        z_stream_s                _inflate;
        bool                      _inflate_eof;  // Got EOF from _base.
        bool                      _inflate_done; // Got Z_STREAM_END from inflate().
        std::unique_ptr<buffer_t> _inflate_in;
        std::unique_ptr<buffer_t> _inflate_out;
    };

    /** An output stream that writes gzipped data.
     */
    struct gzipostream: public std::ostream {
        /** Construct an output stream that writes gzip-compressed data to
         * another ostream.
         */
        gzipostream(
            /// The underlying stream.
            std::ostream& base,
            /// The compression level must be in the range of [0, 9].
            int level = Z_DEFAULT_COMPRESSION)
            : std::ostream{nullptr} {

            if (auto* const base_buf = base.rdbuf(); base_buf != nullptr) {
                _buf = std::make_unique<gzipstreambuf>(base_buf, level);
                rdbuf(_buf.get());
            }
        }

        /** Construct an instance of \ref gzipostream by moving a buffer
         * out of another instance.
         */
        gzipostream(gzipostream&& other)
            : std::ostream{nullptr}
            , _buf{std::move(other._buf)} {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

    private:
        std::unique_ptr<gzipstreambuf> _buf;
    };

    /** An input stream that reads gzipped data.
     */
    struct gunzipistream: public std::istream {
        /** Construct an input stream that reads gzip-compressed data from
         * another istream.
         */
        gunzipistream(std::istream& base)
            : std::istream(nullptr) {

            if (auto* const base_buf = base.rdbuf(); base_buf != nullptr) {
                _buf = std::make_unique<gunzipstreambuf>(base_buf);
                rdbuf(_buf.get());
            }
        }

        /** Construct an instance of \ref gunzipistream by moving a buffer
         * out of another instance.
         */
        gunzipistream(gunzipistream&& other)
            : std::istream(nullptr)
            , _buf(std::move(other._buf)) {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

    private:
        std::unique_ptr<gunzipstreambuf> _buf;
    };
}
