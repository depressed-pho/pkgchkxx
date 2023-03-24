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
    /** A stream buffer that works with a POSIX file descriptor.
     */
    struct fdstreambuf: public std::streambuf {
        /** Construct a stream buffer reading data from / writing data to a
         * file descriptor. The fd will be owned by the buffer, i.e. when
         * it's destructed the fd will also be closed. */
        fdstreambuf(int fd);

        virtual
        ~fdstreambuf();

        /** Explicitly close the file descriptor. The fd will be
         * automatically closed when the buffer is destructed. */
        fdstreambuf*
        close();

    protected:
#if !defined(DOXYGEN)
        virtual int_type
        overflow(int_type ch = traits_type::eof()) override;

        virtual int_type
        underflow() override;

        virtual int_type
        pbackfail(int_type ch = traits_type::eof()) override;
#endif

    private:
        static constexpr int const buf_size = 1024;
        using buffer_t = std::array<char_type, buf_size>;

        int _fd;
        std::optional<buffer_t> _read_buf;
        std::optional<buffer_t> _write_buf;
    };

    /** An output stream that writes data to a POSIX file descriptor.
     */
    struct fdostream: public std::ostream {
        /** Construct an output stream writing data to a file
         * descriptor. The fd will be owned by the stream, i.e. when it's
         * destructed the fd will also be closed. */
        fdostream(int fd)
            : std::ostream(nullptr)
            , _buf(std::make_unique<fdstreambuf>(fd)) {

            rdbuf(_buf.get());
        }

        /** Construct an instance of \ref fdostream by moving a buffer out
         * of another instance. */
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

        /** Explicitly close the file descriptor. The fd will be
         * automatically closed when the stream is destructed. */
        void
        close() {
            if (_buf) {
                _buf->close();
            }
        }

    private:
        std::unique_ptr<fdstreambuf> _buf;
    };

    /** An input stream that reads data from a POSIX file descriptor.
     */
    struct fdistream: public std::istream {
        /** Construct an input stream reading data from a file
         * descriptor. The fd will be owned by the stream, i.e. when it's
         * destructed the fd will also be closed. */
        fdistream(int fd)
            : std::istream(nullptr)
            , _buf(std::make_unique<fdstreambuf>(fd)) {

            rdbuf(_buf.get());
        }

        /** Construct an instance of \ref fdistream by moving a buffer out
         * of another instance. */
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

        /** Explicitly close the file descriptor. The fd will be
         * automatically closed when the stream is destructed. */
        void
        close() {
            if (_buf) {
                _buf->close();
            }
        }

    private:
        std::unique_ptr<fdstreambuf> _buf;
    };

    /** A stream that reads data from / writes data to a POSIX file
     * descriptor.
     */
    struct fdstream: public std::iostream {
        /** Construct a stream reading data from / writing data to a file
         * descriptor. The fd will be owned by the stream, i.e. when it's
         * destructed the fd will also be closed. */
        fdstream(int fd)
            : std::iostream(nullptr)
            , _buf(std::make_unique<fdstreambuf>(fd)) {

            rdbuf(_buf.get());
        }

        /** Construct an instance of \ref fdstream by moving a buffer out
         * of another instance. */
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

        /** Explicitly close the file descriptor. The fd will be
         * automatically closed when the stream is destructed. */
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
