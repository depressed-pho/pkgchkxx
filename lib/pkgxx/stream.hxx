#pragma once

#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>

namespace pkgxx {
    /** Type-erasing wrapper for any output streams deriving from \c
     * std::ostream.
     */
    struct any_ostream: public std::ostream {
        /** \ref any_ostream is DefaultConstructible but
         * default-constructed stream discards everything written to it.
         */
        any_ostream()
            : std::ostream{nullptr} {}

        /** \ref any_ostream is not CopyConstructible. */
        any_ostream(any_ostream const&) = delete;

        /** \ref any_ostream is MoveConstructible. */
        any_ostream(any_ostream&& src)
            : std::ostream{nullptr}
            , _s{std::move(src._s)} {

            rdbuf(_s->rdbuf());
            src.rdbuf(nullptr);
        }

        /** Move an ostream in an instance of \ref any_ostream.
         */
        template <typename OStream>
        any_ostream(OStream&& s)
            : std::ostream{nullptr}
            , _s{std::make_unique<std::decay_t<OStream>>(std::move(s))} {

            static_assert(std::is_rvalue_reference_v<decltype(s)>);
            rdbuf(_s->rdbuf());
        }

        /** Move-assign an instance of \ref any_ostream. The previously
         * stored ostream will be discarded.
         */
        any_ostream&
        operator= (any_ostream&& src) {
            _s = std::move(src._s);
            rdbuf(_s->rdbuf());
            src.set_rdbuf(nullptr);
            return *this;
        }

        /** Move-assign an ostream. The previously stored ostream will be
         * discarded.
         */
        template <typename OStream>
        any_ostream&
        operator= (OStream&& s) {
            static_assert(std::is_rvalue_reference_v<decltype(s)>);
            _s = std::make_unique<std::decay_t<OStream>>(std::move(s));
            rdbuf(_s->rdbuf());
            return *this;
        }

    private:
        std::unique_ptr<std::ostream> _s;
    };

    /** Type-erasing wrapper for any input streams deriving from \c
     * std::istream.
     */
    struct any_istream: public std::istream {
        /** \ref any_istream is DefaultConstructible but
         * default-constructed stream is always at EOF.
         */
        any_istream()
            : std::istream{nullptr} {}

        /** \ref any_istream is not CopyConstructible. */
        any_istream(any_istream const&) = delete;

        /** \ref any_istream is MoveConstructible. */
        any_istream(any_istream&& src)
            : std::istream{nullptr}
            , _s{std::move(src._s)} {

            rdbuf(_s->rdbuf());
            src.set_rdbuf(nullptr);
        }

        /** Move an istream into an instance of \ref any_istream. */
        template <typename IStream>
        any_istream(IStream&& s)
            : std::istream{nullptr}
            , _s{std::make_unique<std::decay_t<IStream>>(std::move(s))} {

            static_assert(std::is_rvalue_reference_v<decltype(s)>);
            rdbuf(_s->rdbuf());
        }

        /** Move-assign an instance of \ref any_istream. The previously
         * stored istream will be discarded.
         */
        any_istream&
        operator= (any_istream&& src) {
            _s = std::move(src._s);
            rdbuf(_s->rdbuf());
            src.set_rdbuf(nullptr);
            return *this;
        }

        /** Move-assign an istream. The previously stored istream will be
         * discarded.
         */
        template <typename IStream>
        any_istream&
        operator= (IStream&& s) {
            static_assert(std::is_rvalue_reference_v<decltype(s)>);
            _s = std::make_unique<std::decay_t<IStream>>(std::move(s));
            rdbuf(_s->rdbuf());
            return *this;
        }

    private:
        std::unique_ptr<std::istream> _s;
    };
}
