#pragma once

#include <array>
#include <istream>
#include <memory>
#include <optional>
#include <streambuf>
#include <bzlib.h>

namespace pkgxx {
    /** A stream buffer that works with bzip2-compressed data. Currently
     * only supports reading operations.
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
        static constexpr int const buf_size = 1024;
        using buffer_t = std::array<char_type, buf_size>;

        std::streambuf* _base;

        bz_stream _bunzip2;
        bool _bunzip2_eof;  // Got EOF from _base.
        bool _bunzip2_done; // Got BZ_STREAM_END from BZ2_bzDecompress().
        std::optional<buffer_t> _bunzip2_in;
        std::optional<buffer_t> _bunzip2_out;
    };

    /** An input stream that reads bzip2-compressed data.
     */
    struct bunzip2istream: public std::istream {
        /** Construct an input stream that reads bzip2-compressed data from
         * another input stream.
         */
        bunzip2istream(std::istream& base)
            : std::istream(nullptr) {

            if (auto* base_buf = base.rdbuf(); base_buf != nullptr) {
                _buf = std::make_unique<bunzip2streambuf>(base_buf);
                rdbuf(_buf.get());
            }
        }

        /** Construct an instance of \ref bunzip2istream by moving a buffer
         * out of another instance. */
        bunzip2istream(bunzip2istream&& other)
            : std::istream(std::move(other))
            , _buf(std::move(other._buf)) {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

    private:
        std::unique_ptr<bunzip2streambuf> _buf;
    };
}
