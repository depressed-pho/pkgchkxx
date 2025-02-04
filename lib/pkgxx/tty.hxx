#pragma once

#include <cassert>
#include <cstddef> // for std::size_t
#include <exception>
#include <optional>
#include <stack>
#include <tuple>
#include <type_traits>
#include <utility>

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
        int width;
        int height;
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
            chunk<T>
            operator() (T&& val) const {
                return chunk<T>(*this, std::forward<T>(val));
            }
        };
    }

#if __cpp_concepts
    template <typename T>
    concept ttystreamlike = requires (T& t, T const& tc, tty::style const& sty) {
        { tc.size() } -> std::optional<dimension<std::size_t>>;
        { t.push_style(sty) } -> void;
        { t.pop_style(sty) } -> void;
    };
#endif

    // I'm not comfortable with bringing it in this scope, but what else
    // can we do?
    using namespace na::literals;

    /** \ref ttystream is a subclass of \c std::iostream that additionally
     * supports operations specific to terminal devices.
     */
    struct ttystream: public fdstream {
        /** Construct a \ref ttystream out of a file descriptor \c
         * fd. Throw \ref not_a_tty If \c fd does not refer to a tty.
         */
        template <typename... Args>
        ttystream(int fd, Args&&... args)
            : fdstream(
                fd,
                na::get("owned"_na = false, std::forward<Args>(args)...))
            , _use_colour(
                na::get("use_colour"_na = default_use_colour(), std::forward<Args>(args)...))
            , _styles({ tty::style {} }) {

            if (!cisatty(fd)) {
                throw not_a_tty(fd);
            }
        }

        virtual ~ttystream() {}

        /** Obtain the size of the terminal. Return \c std::nullopt if
         * obtaining size is not supported on this platform.
         */
        std::optional<dimension<std::size_t>>
        size() const;

        /** This method is actually an implementation detail that cannot be
         * hidden from public. Do not call this directly.
         */
        void
        push_style(tty::style const& sty) {
            assert(!_styles.empty());

            auto combined = _styles.top() + sty;
            apply_style(combined);
            _styles.push(std::move(combined));
        }

        /** This method is actually an implementation detail that cannot be
         * hidden from public. Do not call this directly.
         */
        void
        pop_style() {
            _styles.pop();
            assert(!_styles.empty());
            apply_style(_styles.top());
        }

    protected:
        static bool
        default_use_colour();

    private:
        void
        apply_style(tty::style const& sty);

    private:
        bool _use_colour;
        std::stack<tty::style> _styles; // invariant: always non-empty
    };

    inline ttystream&
    operator<< (ttystream& tty, ttystream& (*manip)(ttystream&)) {
        return manip(tty);
    }

    namespace tty {
        namespace detail {
            struct move_to {
                /// This cannot be optional because of ANSI.
                std::size_t x;
                std::optional<std::size_t> y;
            };

            ttystream&
            operator<< (ttystream& tty, move_to const& m);
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
        ttystream&
        erase_line_from_cursor(ttystream& tty);

        /** A chunk of output that is potentially annotated with styles. \c
         * chunk<T0, T1, ...> contains \ref pkgxx::value_or_ref<T0>, ... so
         * the contained values can either be values or references or a
         * mixture of any combinations. The value of \ref style is borrowed
         * but not copied, so care must be taken about its lifetime.
         */
        template <typename... Ts>
        struct chunk {
            chunk(style const& sty, Ts&&... vs)
                : _sty(sty)
                , _vs(std::forward<Ts>(vs)...) {}

#if __cpp_concepts
            template <ttystreamlike TTY, typename... Us>
#else
            template <typename TTY, typename... Us>
#endif
            friend TTY&
            operator<< (TTY& tty, chunk<Us...> const& rhs) {

                tty.push_style(rhs._sty);

                std::apply([&](auto&&... vs) {
                    ((tty << *vs), ...);
                }, rhs._vs);

                tty.pop_style();

                return tty;
            }

        private:
            style const& _sty;
            std::tuple<value_or_ref<std::decay_t<Ts>>...> _vs;
        };

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
