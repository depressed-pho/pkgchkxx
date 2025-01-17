#pragma once

#include <chrono>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>

#include <pkgxx/fdstream.hxx>
#include <pkgxx/scoped_signal_handler.hxx>

// We know what we are doing! Just don't warn us about these!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-string-literal-operator-template"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <named-parameters.hpp>
#pragma GCC diagnostic pop

namespace pkgxx {
    // I'm not comfortable with bringing these in this scope, but what else
    // can we do?
    using namespace na::literals;
    using namespace std::literals::chrono_literals;

    /** A text-based progress bar to be displayed iff stderr is a tty,
     * based on the algorithm described in
     * https://stackoverflow.com/a/42009090
     *
     * Instances of this class are thread-safe.
     */
    struct progress_bar {
        struct bar_style {
            char begin = '[';
            char fill  = '=';
            char bg    = ' ';
            char tip   = '>';
            char end   = ']';
        };

        /** Creating an instance of \c progress_bar displays a progress
         * bar. "decay_p" is ignored when "show_ETA" is false.
         */
        template <typename... Args>
        progress_bar(std::size_t total, Args&&... args)
            : progress_bar(
                0, total,
                na::get("decay_p"_na      = 0.1        , std::forward<Args>(args)...),
                na::get("show_percent"_na = true       , std::forward<Args>(args)...),
                na::get("show_ETA"_na     = true       , std::forward<Args>(args)...),
                static_cast<bar_style const&>(
                    na::get("bar_style"_na   = bar_style{}, std::forward<Args>(args)...)),
                static_cast<std::chrono::steady_clock::duration const&>(
                    na::get("redraw_rate"_na = 100ms      , std::forward<Args>(args)...))) {}

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

            if (_term_width) {
                _out << '\r'    // Move cursor to column 0.
                     << "\e[K"; // Erase from cursor to the end of line.
            }
            f(_out);
            redraw();
        }

    private:
        progress_bar(
            int, // a dummy parameter to avoid conflicting with the public ctor
            std::size_t total,
            double decay_p,
            bool show_percent,
            bool show_ETA,
            bar_style const& style,
            std::chrono::steady_clock::duration const& redraw_rate);

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

        static std::optional<std::size_t>
        term_width(fdostream const& _out);

    private:
        using mutex_t = std::recursive_mutex;
        using lock_t  = std::lock_guard<mutex_t>;

        mutable mutex_t _mtx;
        double _decay_p;
        bool _show_percent;
        bool _show_ETA;
        bar_style _style;
        std::chrono::steady_clock::duration _redraw_rate;
        fdostream _out;
        // std::nullopt if stderr is not a tty.
        std::optional<std::size_t> _term_width;

        std::chrono::steady_clock::time_point _last_updated;
        std::optional<
            std::chrono::steady_clock::time_point
            > _last_redrew;
        std::optional<double> _slowness_EST; // in seconds
        std::size_t _total;
        std::size_t _done;
        double _weight;

        std::unique_ptr<scoped_signal_handler> _winch_handler;
    };
}
