#pragma once

#include <cassert>
#include <cstddef> // for std::size_t
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stack>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <pkgxx/fdstream.hxx>
#include <pkgxx/value_or_ref.hxx>

// We know what we are doing! Just don't warn us about these!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-string-literal-operator-template"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <named-parameters.hpp>
#pragma GCC diagnostic pop

namespace pkgxx {
    /** Return true iff the given file descriptor refers to a terminal. */
    bool
    cisatty(int fd);

    template <typename T>
    struct dimension {
        T width;
        T height;
    };

    struct not_a_tty: virtual std::runtime_error {
#if !defined(DOXYGEN)
        not_a_tty(int const fd)
            : std::runtime_error("the file descriptor does not refer to a tty")
            , fd(fd) {}
#endif

        int const fd;
    };

    namespace tty {
        namespace detail {
            enum class intensity {
                dull  = 0,
                vivid = 60,
            };

            enum class colour {
                black   = 0,
                red     = 1,
                green   = 2,
                yellow  = 3,
                blue    = 4,
                magenta = 5,
                cyan    = 6,
                white   = 7,
            };

            enum class boldness {
                bold   = 1,
                faint  = 2,
                normal = 22,
            };

            enum class font {
                italics = 3,
                regular = 23,
            };

            enum class underline {
                single = 4,
                none   = 24,
            };
        }

        template <typename... Ts>
        struct chunk;

        struct style {
            std::optional<
                std::pair<detail::intensity, detail::colour>
                > foreground;
            std::optional<
                std::pair<detail::intensity, detail::colour>
                > background;
            std::optional<detail::boldness> boldness;
            std::optional<detail::font> font;
            std::optional<detail::underline> underline;

            style&
            operator+= (style const& rhs) noexcept;

            /** \ref style forms a monoid under its default constructor and
             * the operator \c +. It is not commutative. If \c lhs and \c
             * rhs have conflicting styles, components of \c lhs win.
             */
            friend style
            operator+ (style lhs, style const& rhs) noexcept {
                lhs += rhs;
                return lhs;
            }

            /** The invocation operator attaches the style to a given
             * value. The resulting value, \ref chunk, can be output to
             * streams with the style applied.
             */
            template <typename T>
            chunk<std::remove_reference_t<T>>
            operator() (T&& val) const {
                return chunk<std::remove_reference_t<T>>(
                    std::make_optional(std::cref(*this)),
                    std::forward<T>(val));
            }
        };
    }

    /** \ref ttystreambuf_base is an std::streambuf that is potentially
     * (but not necessarily) connected to tty.
     */
    struct ttystreambuf_base: public virtual std::streambuf {
        enum class how {
            combine,
            no_combine
        };

        virtual ~ttystreambuf_base() = default;

        /** This method is actually an implementation detail that cannot be
         * hidden from public. Do not call this directly.
         */
        virtual std::lock_guard<std::mutex>
        lock() = 0;

        /** Obtain the size of the terminal. Return \c std::nullopt if the
         * underlying stream is not connected to tty, or obtaining size is
         * not supported on this platform.
         */
        virtual std::optional<dimension<std::size_t>>
        term_size() const = 0;

        /** This method is actually an implementation detail that cannot be
         * hidden from public. Do not call this directly.
         */
        virtual void
        push_style(tty::style const& sty, how how_) = 0;

        /** This method is actually an implementation detail that cannot be
         * hidden from public. Do not call this directly.
         */
        virtual void
        pop_style() = 0;
    };

    /** \ref ttystream_base is an std::ostream that is potentially (but not
     * necessarily) connected to tty. Streams of this class must use a
     * subclass of \ref ttystreambuf_base as the stream buffer.
     */
    struct ttystream_base: public virtual std::ostream {
        using how = ttystreambuf_base::how;

        virtual ~ttystream_base() = default;

        /** Obtain the size of the terminal. Return \c std::nullopt if the
         * underlying stream is not connected to tty, or obtaining size is
         * not supported on this platform.
         */
        virtual std::optional<dimension<std::size_t>>
        size() const = 0;

        /** This method is actually an implementation detail that cannot be
         * hidden from public. Do not call this directly.
         */
        virtual void
        push_style(tty::style const& sty, how how_) = 0;

        /** This method is actually an implementation detail that cannot be
         * hidden from public. Do not call this directly.
         */
        virtual void
        pop_style() = 0;
    };

    // I'm not comfortable with bringing it in this scope, but what else
    // can we do?
    using namespace na::literals;

    /** \ref ttystream is a subclass of \c std::iostream that additionally
     * supports operations specific to terminal devices.
     */
    struct ttystream: public virtual ttystream_base
                    , public virtual fdstream {
        /** Construct a \ref ttystream out of a file descriptor \c
         * fd. Throw \ref not_a_tty if \c fd does not refer to a tty.
         */
        template <typename... Args>
        ttystream(int fd, Args&&... args)
            : std::istream(nullptr)
            , fdstream(
                fd,
                na::get("owned"_na = false, std::forward<Args>(args)...))
            , _use_colour(
                na::get("use_colour"_na = default_use_colour(), std::forward<Args>(args)...))
            , _styles({ tty::style {} }) {

            if (!cisatty(fd)) {
                throw not_a_tty(fd);
            }
        }

        virtual ~ttystream() = default;

        virtual std::optional<dimension<std::size_t>>
        size() const override;

        virtual void
        push_style(tty::style const& sty, how how_) override {
            if (!_use_colour) {
                return;
            }
            assert(!_styles.empty());

            switch (how_) {
            case how::combine:
                {
                    auto combined = sty + _styles.top();
                    apply_style(combined);
                    _styles.push(std::move(combined));
                }
                break;

            case how::no_combine:
                apply_style(sty);
                _styles.push(sty);
                break;
            }
        }

        virtual void
        pop_style() override {
            if (!_use_colour) {
                return;
            }
            _styles.pop();
            assert(!_styles.empty());
            apply_style(_styles.top());
        }

    private:
        static bool
        default_use_colour();

        void
        apply_style(tty::style const& sty);

    private:
        bool _use_colour;
        std::stack<tty::style> _styles; // invariant: always non-empty
    };

    inline ttystream_base&
    operator<< (ttystream_base& tty, ttystream_base& (*manip)(ttystream_base&)) {
        return manip(tty);
    }

    namespace tty {
        template <typename>
        struct is_chunk_t: std::false_type {};

        template <typename... Args>
        struct is_chunk_t<chunk<Args...>>: std::true_type{};

        template <typename T>
        constexpr bool is_chunk_v = is_chunk_t<T>::value;
    }

    template <typename T,
              typename = std::enable_if_t<!tty::is_chunk_v<T>>>
    ttystream_base&
    operator<< (ttystream_base& tty, T const& val) {
        static_cast<std::ostream&>(tty) << val;
        return tty;
    }

    template <typename T,
              typename = std::enable_if_t<!tty::is_chunk_v<T>>>
    ttystream_base&&
    operator<< (ttystream_base&& tty, T const& val) {
        static_cast<std::ostream&>(tty) << val;
        return std::move(tty);
    }

    /** \ref ttystream is a subclass of \c std::streambuf that potentially
     * (but not necessarily) refers to a tty.
     */
    struct maybe_ttystreambuf: public virtual ttystreambuf_base
                             , public virtual std::streambuf {
        template <typename... Args>
        maybe_ttystreambuf(int fd, Args&&... args)
            : _out([&] {
                       if (cisatty(fd)) {
                           return decltype(_out)(
                               std::in_place_type<ttystream>,
                               fd, std::forward<Args>(args)...);
                       }
                       else {
                           return decltype(_out)(
                               std::in_place_type<fdostream>,
                               fd, na::get("owned"_na = false, std::forward<Args>(args)...));
                       }
                   }()) {}

        virtual std::lock_guard<std::mutex>
        lock() override {
            return std::lock_guard(_mtx);
        }

        virtual std::optional<pkgxx::dimension<std::size_t>>
        term_size() const override;

        virtual void
        push_style(pkgxx::tty::style const& sty, ttystream_base::how how_) override;

        virtual void
        pop_style() override;

    protected:
        virtual int
        sync() override;

        virtual int_type
        overflow(int_type ch = traits_type::eof()) override;

        virtual std::streamsize
        xsputn(const char_type* s, std::streamsize count) override;

    private:
        std::mutex _mtx;
        std::variant<ttystream, fdostream> _out;
    };

    /** \ref ttystream is a subclass of \c ttystream_base that potentially
     * (but not necessarily) refers to a tty.
     */
    struct maybe_ttystream: public virtual ttystream_base
                          , public virtual std::ostream {
        maybe_ttystream()
            : std::ostream(nullptr) {}

        /** Construct a \ref ttystream out of a file descriptor \c
         * fd. Unlike \ref ttystream, the constructor does not throw \ref
         * not_a_tty even if \c fd does not refer to a tty.
         */
        template <typename... Args>
        maybe_ttystream(int fd, Args&&... args)
            : std::ostream(nullptr)
            , _buf(std::in_place, fd, std::forward<Args>(args)...) {

            rdbuf(&_buf.value());
        }

        virtual ~maybe_ttystream() = default;

        virtual std::optional<dimension<std::size_t>>
        size() const override {
            return _buf ? _buf->term_size() : std::nullopt;
        }

        virtual void
        push_style(tty::style const& sty, how how_) override {
            if (_buf) {
                return _buf->push_style(sty, how_);
            }
        }

        virtual void
        pop_style() override {
            if (_buf) {
                return _buf->pop_style();
            }
        }

    private:
        std::optional<maybe_ttystreambuf> _buf;
    };

    /** This is like \c std::syncbuf but is for \ref ttystream_base.
     */
    struct maybe_tty_syncbuf: public virtual ttystreambuf_base
                            , public virtual std::streambuf {
        maybe_tty_syncbuf(ttystreambuf_base& out)
            : _out(out) {}

        virtual ~maybe_tty_syncbuf();

        // Don't call this.
        [[noreturn]] virtual std::lock_guard<std::mutex>
        lock() override;

        virtual std::optional<pkgxx::dimension<std::size_t>>
        term_size() const override;

        virtual void
        push_style(pkgxx::tty::style const& sty, ttystream_base::how how_) override;

        virtual void
        pop_style() override;

    protected:
        virtual int
        sync() override;

        virtual int_type
        overflow(int_type ch = traits_type::eof()) override;

        virtual std::streamsize
        xsputn(const char_type* s, std::streamsize count) override;

    private:
        struct sync_cmd {};
        struct write_cmd: public std::string {
            using std::string::string;
        };
        struct push_style_cmd: public tty::style {
            push_style_cmd(tty::style const& sty, ttystream_base::how how_)
                : tty::style(sty)
                , how_(how_) {}

            ttystream_base::how how_;
        };
        struct pop_style_cmd {};

        ttystreambuf_base& _out;
        std::vector<
            std::variant<
                sync_cmd,
                write_cmd,
                push_style_cmd,
                pop_style_cmd
                >
            > _cmds;
    };

    /** This is like \c std::osyncstream but is for \ref maybe_ttystream.
     */
    struct maybe_tty_osyncstream: public virtual ttystream_base
                                , public virtual std::ostream {
        maybe_tty_osyncstream()
            : std::ostream(nullptr)
            , _buf(nullptr) {}

        maybe_tty_osyncstream(ttystream_base& out)
            : std::ostream(nullptr)
            , _buf(out.rdbuf()
                   ? std::make_unique<maybe_tty_syncbuf>(
                       dynamic_cast<ttystreambuf_base&>(*out.rdbuf()))
                   : nullptr) {

            rdbuf(_buf.get());
        }

        maybe_tty_osyncstream(maybe_tty_osyncstream&& other)
            : std::ostream(nullptr)
            , _buf(std::move(other._buf)) {

            other.set_rdbuf(nullptr);
            rdbuf(_buf.get());
        }

        virtual ~maybe_tty_osyncstream() = default;

        virtual std::optional<dimension<std::size_t>>
        size() const override;

        virtual void
        push_style(tty::style const& sty, how how_) override;

        virtual void
        pop_style() override;

    private:
        std::unique_ptr<maybe_tty_syncbuf> _buf;
    };

    namespace tty {
        namespace detail {
            struct move_to {
                /// This cannot be optional because of ANSI.
                std::size_t x;
                std::optional<std::size_t> y;
            };

            ttystream_base&
            operator<< (ttystream_base& tty, move_to const& m);
        }

        /** An output manipulator that moves the cursor to a given
         * 0-indexed column.
         */
        inline detail::move_to
        move_x(std::size_t const col) {
            return detail::move_to { col, {} };
        }

        /** An output manipulator that erases the current line from the
         * cursor to the end.
         */
        ttystream_base&
        erase_line_from_cursor(ttystream_base& tty);

        /** A chunk of output that is potentially annotated with styles. \c
         * chunk<T0, T1, ...> contains \ref pkgxx::value_or_ref<T0 const>,
         * ... so the contained values can either be values or references
         * or a mixture of any combinations. The value of \ref style is
         * borrowed but not copied, so care must be taken about its
         * lifetime.
         */
        template <typename... Ts>
        struct chunk {
            template <typename... Us>
            chunk(std::optional<std::reference_wrapper<style const>>&& sty, Us&&... vs)
                : _sty(std::move(sty))
                , _vs(std::forward<Us>(vs)...) {}

            /** Append something to a chunk.
             */
            template <typename U>
            friend chunk<chunk<Ts...>, std::remove_reference_t<U>>
            operator<< (chunk<Ts...>&& lhs, U&& rhs) {
                return chunk<chunk<Ts...>, std::remove_reference_t<U>>(
                    std::nullopt,
                    std::move(lhs),
                    std::forward<U>(rhs));
            }

            /** Special case for appending a string literal to a chunk. We
             * need this due to the way how chunks are implemented.
             */
            template <std::size_t N>
            friend chunk<chunk<Ts...>, std::string_view>
            operator<< (chunk<Ts...>&& lhs, char const (&rhs)[N]) {
                return chunk<chunk<Ts...>, std::string_view>(
                    std::nullopt,
                    std::move(lhs),
                    rhs);
            }

            friend ttystream_base&
            operator<< (ttystream_base& tty, chunk<Ts...> const& rhs) {

                if (rhs._sty) {
                    tty.push_style(*rhs._sty, ttystream_base::how::combine);
                }

                std::apply([&](auto&&... vs) {
                    ((tty << *vs), ...);
                }, rhs._vs);

                if (rhs._sty) {
                    tty.pop_style();
                }

                return tty;
            }

            friend ttystream_base&&
            operator<< (ttystream_base&& tty, chunk<Ts...> const& rhs) {
                static_cast<ttystream_base&>(tty) << rhs;
                return std::move(tty);
            }

        private:
            std::optional<std::reference_wrapper<style const>> _sty;
            std::tuple<
                value_or_ref<
                    std::add_const_t<Ts>
                    >...
                > _vs;
        };

        namespace literals {
            inline chunk<std::string_view>
            operator"" _ch(char const* str, std::size_t len) {
                return chunk<std::string_view>(
                    std::nullopt, std::string_view(str, len));
            }

            inline chunk<char>
            operator"" _ch(char c) {
                // std::move() is necessary here, otherwise
                // value_or_ref<char> would think it's an lvalue reference
                // and only borrow this temporary char but not copy it.
                return chunk<char>(std::nullopt, std::move(c));
            }
        }

#if __cplusplus >= 202002L
        // THINKME: Remove this CPP conditional when we switch to C++20.
        using enum detail::colour;
#else
        constexpr inline detail::colour black   = detail::colour::black;
        constexpr inline detail::colour red     = detail::colour::red;
        constexpr inline detail::colour green   = detail::colour::green;
        constexpr inline detail::colour yellow  = detail::colour::yellow;
        constexpr inline detail::colour blue    = detail::colour::blue;
        constexpr inline detail::colour magenta = detail::colour::magenta;
        constexpr inline detail::colour cyan    = detail::colour::cyan;
        constexpr inline detail::colour white   = detail::colour::white;
#endif

        style dull_colour(detail::colour const c);
        style colour(detail::colour const c);
        style dull_bg_colour(detail::colour const c);
        style bg_colour(detail::colour const c);

        extern style const bold;
        extern style const faint;
        extern style const italicised;
        extern style const underlined;
    }
}
