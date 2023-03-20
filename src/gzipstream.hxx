#pragma once

#include <array>
#include <istream>
#include <memory>
#include <optional>
#include <streambuf>
#include <zlib.h>

namespace pkg_chk {
    /** Currently only supports reading operations.
     */
    struct gunzipstreambuf: public std::streambuf {
        gunzipstreambuf(std::streambuf* base);
        virtual ~gunzipstreambuf();

    protected:
        virtual int_type
        underflow() override;

        virtual int_type
        pbackfail(int_type ch = traits_type::eof()) override;

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

    struct gunzipistream: public std::istream {
        /** Construct an input stream that reads gzip-compressed data from
         * an underlying istream.
         */
        gunzipistream(std::istream& base)
            : std::istream(nullptr) {

            if (auto* base_buf = base.rdbuf(); base_buf != nullptr) {
                _buf = std::make_unique<gunzipstreambuf>(base_buf);
                rdbuf(_buf.get());
            }
        }

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
