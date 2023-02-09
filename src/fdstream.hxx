#pragma once

/* Streaming I/O based on POSIX file descriptors.
 */

#include <array>
#include <memory>
#include <ostream>
#include <istream>
#include <streambuf>

namespace pkg_chk {
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
        overflow(int_type ch = traits_type::eof());

        virtual int_type
        underflow();

        virtual int_type
        pbackfail(int_type ch = traits_type::eof());

    private:
        static constexpr int const buf_size = 1024;
        using buffer_t = std::array<char_type, buf_size>;

        int _fd;
        std::unique_ptr<buffer_t> _read_buf;
        std::unique_ptr<buffer_t> _write_buf;
    };

    /* A subclass of std::ostream that works with a POSIX file descriptor.
     */
    struct fdostream: public std::ostream {
        /* The file descriptor will be owned by the buffer, i.e. when it's
         * destructed the fd will also be closed. */
        fdostream(int fd)
            : std::ostream(nullptr)
            , _buf(fd) {

            rdbuf(&_buf);
        }

        virtual
        ~fdostream() {
            close();
        }

        void
        close() {
            _buf.close();
        }

    private:
        fdstreambuf _buf;
    };

    /* A subclass of std::istream that works with a POSIX file descriptor.
     */
    struct fdistream: public std::istream {
        /* The file descriptor will be owned by the buffer, i.e. when it's
         * destructed the fd will also be closed. */
        fdistream(int fd)
            : std::istream(nullptr)
            , _buf(fd) {

            rdbuf(&_buf);
        }

        virtual
        ~fdistream() {
            close();
        }

        void
        close() {
            _buf.close();
        }

    private:
        fdstreambuf _buf;
    };

    /* A subclass of std::iostream that works with a POSIX file descriptor.
     */
    struct fdstream: public std::iostream {
        /* The file descriptor will be owned by the buffer, i.e. when it's
         * destructed the fd will also be closed. */
        fdstream(int fd)
            : std::iostream(nullptr)
            , _buf(fd) {

            rdbuf(&_buf);
        }

        virtual
        ~fdstream() {
            close();
        }

        void
        close() {
            _buf.close();
        }

    private:
        fdstreambuf _buf;
    };
}
