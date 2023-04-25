#pragma once

#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>

namespace pkgxx {
    /// Type-erasing wrapper for any types deriving from \c std::ostream.
    struct any_ostream: public std::ostream {
        /// \ref any_ostream is not DefaultConstructible.
        any_ostream() = delete;

        /// \ref any_ostream is not CopyConstructible.
        any_ostream(any_ostream const&) = delete;

        /// \ref any_ostream is MoveConstructible.
        any_ostream(any_ostream&& src)
            : std::ostream(std::move(src))
            , _buf(std::move(src._buf)) {

            rdbuf(&_buf);
        }

        /// Forward an ostream into an instance of \ref any_ostream.
        template <typename OStream>
        any_ostream(OStream&& s)
            : std::ostream(nullptr)
            , _buf(std::forward<OStream>(s)) {

            static_assert(std::is_base_of_v<std::ostream, OStream>);
            rdbuf(&_buf);
        }

        /// Forward-assign an ostream. The previously stored ostream will
        /// be discarded.
        template <typename OStream>
        any_ostream&
        operator== (OStream&& s) {
            static_assert(std::is_base_of_v<std::ostream, OStream>);
            _buf = std::forward<OStream>(s);
            return *this;
        }

    private:
        struct proxy_buf: public std::streambuf {
            proxy_buf() = delete;

            template <typename OStream>
            proxy_buf(OStream&& s)
                : _sp(std::make_unique<OStream>(std::forward<OStream>(s))) {}

            template <typename OStream>
            proxy_buf&
            operator== (OStream&& s) {
                _sp = std::make_unique<OStream>(std::forward<OStream>(s));
                return *this;
            }

        protected:
            virtual int_type
            overflow(int_type ch = traits_type::eof()) override {
                if (!traits_type::eq_int_type(ch, traits_type::eof())) {
                    _sp->put(traits_type::to_char_type(ch));
                }
                return ch;
            }

            virtual std::streamsize
            xsputn(const char_type* s, std::streamsize count) override {
                _sp->write(s, count);
                return count;
            }

        private:
            std::unique_ptr<std::ostream> _sp;
        };

        proxy_buf _buf;
    };
}
