#pragma once

#include <array>
#include <istream>
#include <memory>
#include <stdexcept>
#include <streambuf>
#include <bzlib.h>

namespace pkgxx {
    struct bzip2_error: public std::runtime_error {
        bzip2_error(int code);

        int code;

    private:
        static std::string
        message(int code);
    };

    /** A stream buffer that compresses data with bzip2. Currently only
     * supports write operations. That is, one can write uncompressed data
     * to this buffer and compress it, but one cannot read uncompressed
     * data from another buffer to obtain compressed data from this buffer.
     */
    struct bzip2streambuf: public std::streambuf {
        /** Construct a stream buffer writing bzip2-compressed data to
         * another stream buffer.
         */
        bzip2streambuf(
            /// The underlying buffer.
            std::streambuf* base,
            /// Block size in 100 k; must be in the range of [1, 9].
            int block_size = 9);

        virtual ~bzip2streambuf();

    protected:
#if !defined(DOXYGEN)
        virtual int_type
        overflow(int_type ch = traits_type::eof()) override;
#endif

    private:
        static constexpr int const buf_size = 65536;
        using buffer_t = std::array<char_type, buf_size>;

        std::streambuf*           _base;
        bz_stream                 _bzip2;
        std::unique_ptr<buffer_t> _bzip2_in;
        std::unique_ptr<buffer_t> _bzip2_out;
    };

    /** A stream buffer that decompresses bzip2-compressed data. Currently
     * only supports read operations. That is, one can read compressed data
     * from another buffer and decompress it, but one cannot write
     * compressed data to this buffer and transfer decompressed data to
     * another buffer.
     */
    struct bunzip2streambuf: public std::streambuf {
        /** Construct a stream buffer that reads bzip2-compressed data from
         * another stream buffer.
         */
        bunzip2streambuf(std::streambuf* base);
        virtual ~bunzip2streambuf();

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
        bz_stream                 _bunzip2;
        bool                      _bunzip2_eof;  // Got EOF from _base.
        bool                      _bunzip2_done; // Got BZ_STREAM_END from BZ2_bzDecompress().
        std::unique_ptr<buffer_t> _bunzip2_in;
        std::unique_ptr<buffer_t> _bunzip2_out;
    };

    /** An output stream that writes bzip2-compressed data.
     */
    struct bzip2ostream: public std::ostream {
        /** Construct an output stream that writes bzip2-compressed data to
         * another output stream.
         */
        bzip2ostream(std::ostream& base)
            : std::ostream(nullptr) {

            if (auto* const base_buf = base.rdbuf(); base_buf != nullptr) {
                _buf = std::make_unique<bzip2streambuf>(base_buf);
                rdbuf(_buf.get());
            }
        }

        /** Construct an instance of \ref bzip2ostream by moving a buffer
         * out of another instance.
         */
        bzip2ostream(bzip2ostream&& other)
            : std::ostream(nullptr)
            , _buf(std::move(other._buf)) {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

    private:
        std::unique_ptr<bzip2streambuf> _buf;
    };

    /** An input stream that reads bzip2-compressed data.
     */
    struct bunzip2istream: public std::istream {
        /** Construct an input stream that reads bzip2-compressed data from
         * another input stream.
         */
        bunzip2istream(std::istream& base)
            : std::istream(nullptr) {

            if (auto* const base_buf = base.rdbuf(); base_buf != nullptr) {
                _buf = std::make_unique<bunzip2streambuf>(base_buf);
                rdbuf(_buf.get());
            }
        }

        /** Construct an instance of \ref bunzip2istream by moving a buffer
         * out of another instance.
         */
        bunzip2istream(bunzip2istream&& other)
            : std::istream(nullptr)
            , _buf(std::move(other._buf)) {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

    private:
        std::unique_ptr<bunzip2streambuf> _buf;
    };
}
