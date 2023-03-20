#pragma once

/* Streaming I/O based on POSIX file descriptors.
 */

#include <array>
#include <memory>
#include <optional>
#include <ostream>
#include <istream>
#include <streambuf>
#include <utility>

namespace pkgxx {
    /* A subclass of std::streambuf that works with a POSIX file
     * descriptor.
     */
    struct fdstreambuf: public std::streambuf {
        /* The file descriptor will be owned by the buffer, i.e. when it's
         * destructed the fd will also be closed. */
        fdstreambuf(int fd);

        virtual
        ~fdstreambuf();

        fdstreambuf*
        close();

    protected:
        virtual int_type
        overflow(int_type ch = traits_type::eof()) override;

        virtual int_type
        underflow() override;

        virtual int_type
        pbackfail(int_type ch = traits_type::eof()) override;

    private:
        static constexpr int const buf_size = 1024;
        using buffer_t = std::array<char_type, buf_size>;

        int _fd;
        std::optional<buffer_t> _read_buf;
        std::optional<buffer_t> _write_buf;
    };

    /* A subclass of std::ostream that works with a POSIX file descriptor.
     */
    struct fdostream: public std::ostream {
        /* The file descriptor will be owned by the buffer, i.e. when it's
         * destructed the fd will also be closed. */
        fdostream(int fd)
            : std::ostream(nullptr)
            , _buf(std::make_unique<fdstreambuf>(fd)) {

            rdbuf(_buf.get());
        }

        fdostream(fdostream&& other)
            : std::ostream(std::move(other))
            , _buf(std::move(other._buf)) {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

        virtual
        ~fdostream() {
            close();
        }

        void
        close() {
            if (_buf) {
                _buf->close();
            }
        }

    private:
        std::unique_ptr<fdstreambuf> _buf;
    };

    /* A subclass of std::istream that works with a POSIX file descriptor.
     */
    struct fdistream: public std::istream {
        /* The file descriptor will be owned by the buffer, i.e. when it's
         * destructed the fd will also be closed. */
        fdistream(int fd)
            : std::istream(nullptr)
            , _buf(std::make_unique<fdstreambuf>(fd)) {

            rdbuf(_buf.get());
        }

        fdistream(fdistream&& other)
            : std::istream(std::move(other))
            , _buf(std::move(other._buf)) {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

        virtual
        ~fdistream() {
            close();
        }

        void
        close() {
            if (_buf) {
                _buf->close();
            }
        }

    private:
        std::unique_ptr<fdstreambuf> _buf;
    };

    /* A subclass of std::iostream that works with a POSIX file descriptor.
     */
    struct fdstream: public std::iostream {
        /* The file descriptor will be owned by the buffer, i.e. when it's
         * destructed the fd will also be closed. */
        fdstream(int fd)
            : std::iostream(nullptr)
            , _buf(std::make_unique<fdstreambuf>(fd)) {

            rdbuf(_buf.get());
        }

        fdstream(fdstream&& other)
            : std::iostream(std::move(other))
            , _buf(std::move(other._buf)) {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

        virtual
        ~fdstream() {
            close();
        }

        void
        close() {
            if (_buf) {
                _buf->close();
            }
        }

    private:
        std::unique_ptr<fdstreambuf> _buf;
    };
}
