#pragma once

#include <array>
#include <istream>
#include <memory>
#include <optional>
#include <streambuf>
#include <bzlib.h>

namespace pkg_chk {
    /** Currently only supports reading operations. */
    struct bunzip2streambuf: public std::streambuf {
        bunzip2streambuf(std::streambuf* base);
        virtual ~bunzip2streambuf();

    protected:
        virtual int_type
        underflow();

        virtual int_type
        pbackfail(int_type ch = traits_type::eof());

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

    struct bunzip2istream: public std::istream {
        /** Construct an input stream that reads bzip2-compressed data from
         * an underlying istream.
         */
        bunzip2istream(std::istream& base)
            : std::istream(nullptr) {

            if (auto* base_buf = base.rdbuf(); base_buf != nullptr) {
                _buf = std::make_unique<bunzip2streambuf>(base_buf);
                rdbuf(_buf.get());
            }
        }

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
