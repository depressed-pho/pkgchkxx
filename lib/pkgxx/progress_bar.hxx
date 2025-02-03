#pragma once

#include <cassert>
#include <chrono>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <type_traits>

#include <pkgxx/tty.hxx>
#include <pkgxx/value_or_ref.hxx>

// We know what we are doing! Just don't warn us about these!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-string-literal-operator-template"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <named-parameters.hpp>
#pragma GCC diagnostic pop

namespace pkgxx {
    // I'm not comfortable with bringing these in this scope, but what else
    // can we do?
    using namespace na::literals;
    using namespace std::literals::chrono_literals;

    /** A text-based progress bar, based on the algorithm described in
     * https://stackoverflow.com/a/42009090
     *
     * Instances of this class are thread-safe.
     */
    struct progress_bar {
        struct bar_style {
            char begin = '[';
            char fill  = ':';
            char bg    = ' ';
            char tip   = ':';
            char end   = ']';
        };

        /** Creating an instance of \c progress_bar displays a progress
         * bar. "decay_p" is ignored when "show_ETA" is false. \c "output"
         * is defaulted to stderr, and if it's not a \ref ttystream the
         * progress bar will not be actually drawn.
         */
        template <typename... Args>
        progress_bar(std::size_t total, Args&&... args)
            : progress_bar(
                0, total,
                static_cast<std::optional<std::reference_wrapper<std::ostream>> const&>(
                    na::get("output"_na = std::nullopt, std::forward<Args>(args)...)),
                na::get("decay_p"_na      = 0.1 , std::forward<Args>(args)...),
                na::get("show_percent"_na = true, std::forward<Args>(args)...),
                na::get("show_ETA"_na     = true, std::forward<Args>(args)...),
                static_cast<bar_style const&>(
                    na::get("bar_style"_na   = bar_style{}, std::forward<Args>(args)...)),
                static_cast<std::chrono::steady_clock::duration const&>(
                    na::get("redraw_rate"_na = 200ms      , std::forward<Args>(args)...))) {}

        ~progress_bar();

        progress_bar& operator++ ()    { return *this += 1; }
        progress_bar& operator++ (int) { return *this += 1; }
        progress_bar& operator+= (std::size_t delta);
        progress_bar& operator = (std::size_t done);

        /** Print a message. The function \c f is assumed to print a
         * newline at the end.
         */
        template <typename F>
        void
        message(F const& f) {
            lock_t lk(_mtx);

            static_assert(std::is_invocable_v<F, std::ostream&>);
            static_assert(std::is_same_v<void, std::invoke_result_t<F, std::ostream&>>);

            if (should_draw()) {
                tty() << tty::move_x(0)
                      << tty::erase_line_from_cursor;
            }
            f(*_output);
            redraw();
        }

    private:
        progress_bar(
            int, // a dummy parameter to avoid conflicting with the public ctor
            std::size_t total,
            std::optional<std::reference_wrapper<std::ostream>> const& output,
            double decay_p,
            bool show_percent,
            bool show_ETA,
            bar_style const& style,
            std::chrono::steady_clock::duration const& redraw_rate);

        static value_or_ref<std::ostream>
        default_output();

        /** Return a nullptr if the output stream is not actually a
         * \ref ttystream.
         */
        ttystream*
        ttyp() {
            return dynamic_cast<ttystream*>(_output.get());
        }

        /** Abort if ttyp() returns nullptr. */
        ttystream&
        tty() {
            auto const ptr = ttyp();
            assert(ptr);
            return *ptr;
        }

        /** Should we actually draw a progress bar? */
        bool
        should_draw() const {
            return _term_size.has_value();
        }

        void
        redraw(bool force = false);

        void
        redraw(std::initializer_list<std::string> const& postfix);

        double
        progress() const;

        std::string
        format_bar(std::size_t width) const;

        std::string
        format_percentage() const;

        std::string
        format_ETA() const;

    private:
        using mutex_t = std::recursive_mutex;
        using lock_t  = std::lock_guard<mutex_t>;

        mutable mutex_t _mtx;
        value_or_ref<std::ostream> _output;
        double _decay_p;
        bool _show_percent;
        bool _show_ETA;
        bar_style _style;
        std::chrono::steady_clock::duration _redraw_rate;
        // std::nullopt if _output is not a tty.
        std::optional<dimension<std::size_t>> _term_size;

        std::chrono::steady_clock::time_point _last_updated;
        std::optional<
            std::chrono::steady_clock::time_point
            > _last_redrew;
        std::optional<double> _slowness_EST; // in seconds
        std::size_t _total;
        std::size_t _done;
        double _weight;
    };
}
