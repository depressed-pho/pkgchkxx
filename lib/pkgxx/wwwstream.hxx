#pragma once

#include <array>
#include <exception>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <streambuf>
#include <fetch.h>

namespace pkgxx {
    struct remote_file_error: std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct remote_file_unavailable: remote_file_error {
        using remote_file_error::remote_file_error;
    };

    /** Currently only supports reading operations. */
    struct wwwstreambuf: public std::streambuf {
        wwwstreambuf(std::string const& url);

    protected:
        virtual int_type
        underflow() override;

        virtual int_type
        pbackfail(int_type ch = traits_type::eof()) override;

    private:
        struct fetchIO_deleter {
            void
            operator() (fetchIO* fio) const {
                fetchIO_close(fio);
            }
        };

        static constexpr int const buf_size = 1024;
        using buffer_t = std::array<char_type, buf_size>;

        std::unique_ptr<fetchIO, fetchIO_deleter> _fio;
        std::optional<buffer_t> _read_buf;
    };

    /* A subclass of std::istream that fetches a resource with URL.
     */
    struct wwwistream: public std::istream {
        wwwistream(std::string const& url)
            : std::istream(nullptr)
            , _buf(std::make_unique<wwwstreambuf>(url)) {

            rdbuf(_buf.get());
        }

        wwwistream(wwwistream&& other)
            : std::istream(std::move(other))
            , _buf(std::move(other._buf)) {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

    private:
        std::unique_ptr<wwwstreambuf> _buf;
    };
}
