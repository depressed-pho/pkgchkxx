#include "config.h"

#include <cmath>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <signal.h>
#include <sstream>
#include <unistd.h>

#include "progress_bar.hxx"
#include "signal.hxx"
#include "tty.hxx"

namespace {
    std::sig_atomic_t got_SIGWINCH = false;

    std::once_flag sigwinch_handler_installed;

    void
    sigwinch_handler(int) {
        got_SIGWINCH = true;
    }
}

namespace pkgxx {
    progress_bar::progress_bar(
        int,
        std::size_t total,
        value_or_ref<ttystream_base>&& output,
        double decay_p,
        bool show_percent,
        bool show_ETA,
        bar_style const& style,
        std::chrono::steady_clock::duration const& redraw_rate)
        : _output(std::move(output))
        , _decay_p(decay_p)
        , _show_percent(show_percent)
        , _show_ETA(show_ETA)
        , _style(std::move(style))
        , _redraw_rate(redraw_rate)
        , _term_size(_output->size())
        , _last_updated(std::chrono::steady_clock::now())
        , _total(total)
        , _done(0)
          // _weight is a function of _total and decay_p. Computing it
          // isn't so cheap so we cache it.
        , _weight(std::exp(-1 / (static_cast<double>(_total) * _decay_p))) {

        if (_decay_p <= 0) {
            throw std::runtime_error(
                "decay_p must be positive and should not be greater than 1");
        }
#if HAVE_DECL_SIGWINCH
        if (_term_size) {
            // SIGWINCH exists everywhere in practice, but it's
            // nevertheless non-standard.
            std::call_once(
                sigwinch_handler_installed,
                []() {
                    csigaction sa;
                    sa.handler() = sigwinch_handler;
                    sa.install(SIGWINCH);
                });
        }
#endif
        redraw(true);
    }

    progress_bar::~progress_bar() {
        if (should_draw()) {
            *_output << tty::move_x(0)
                     << tty::erase_line_from_cursor
                     << std::flush;
        }
    }

    progress_bar&
    progress_bar::operator+= (std::size_t delta) {
        lock_t lk(_mtx);
        return *this = _done + delta;
    }

    progress_bar&
    progress_bar::operator= (std::size_t done) {
        lock_t lk(_mtx);

        if (done < _done) {
            throw std::runtime_error("progress must be monotonically increasing");
        }
        else if (done == _done) {
            return *this; // no-op
        }
        else if (done > _total) {
            throw std::runtime_error("progress must not be greater than total");
        }

        auto const now      = std::chrono::steady_clock::now();
        auto const slowness = // in seconds
            static_cast<double>(_total) *
            std::chrono::duration<double>(now - _last_updated).count();

        if (!_slowness_EST) {
            _slowness_EST = slowness;
        }
        else {
            _slowness_EST = *_slowness_EST * _weight + slowness * (1.0 - _weight);
        }
        _done         = done;
        _last_updated = now;

        redraw();
        return *this;
    }

    value_or_ref<ttystream_base>
    progress_bar::default_output() {
        return value_or_ref<ttystream_base>(
            std::in_place_type<maybe_ttystream>, STDERR_FILENO, "owned"_na = false);
    }

    void
    progress_bar::redraw(bool force) {
        if (got_SIGWINCH) {
            force = true;
            got_SIGWINCH = false;
            _term_size = _output->size();
        }

        if (!should_draw()) {
            return;
        }

        auto const now = std::chrono::steady_clock::now();
        if (!force && _last_redrew) {
            auto const elapsed = now - *_last_redrew;
            if (elapsed < _redraw_rate) {
                return;
            }
        }

        if (_show_percent) {
            auto const pct = format_percentage();

            if (_show_ETA) {
                redraw({format_percentage(), format_ETA()});
            }
            else {
                redraw({format_percentage()});
            }
        }
        else {
            if (_show_ETA) {
                redraw({format_ETA()});
            }
            else {
                redraw({});
            }
        }

        _last_redrew = now;
    }

    void
    progress_bar::redraw(std::initializer_list<std::string> const& postfix) {
        assert(_term_size);

        std::size_t bar_width = _term_size->width;
        for (auto const& elem: postfix) {
            if (bar_width < 1 + elem.length()) {
                // The terminal is too narrow.
                *_output << tty::move_x(0)
                         << tty::erase_line_from_cursor
                         << std::flush;
                return;
            }
            else {
                bar_width -= 1 + elem.length();
            }
        }

        // No need to erase the line. We can just overwrite it.
        *_output << tty::move_x(0);
        _output->push_style(_style.base_sty, ttystream_base::how::combine);
        render_bar(bar_width);
        for (auto const& elem: postfix) {
            *_output << ' ' << elem;
        }
        _output->pop_style();
        *_output << std::flush;
    }

    double
    progress_bar::progress() const {
        if (_total == 0) {
            return 1.0; // special case
        }
        else {
            return static_cast<double>(_done) / static_cast<double>(_total);
        }
    }

    void
    progress_bar::render_bar(std::size_t width) {
        if (width < 2) {
            // The terminal is too narrow.
            return;
        }

        std::size_t const prog =
            static_cast<std::size_t>(
                std::floor(progress() * static_cast<double>(width - 2)));

        *_output << _style.begin_sty(_style.begin);
        for (std::size_t i = 0; i < width - 2; i++) {
            if (i < prog) {
                *_output << _style.fill_sty(_style.fill);
            }
            else if (i == prog) {
                *_output << _style.tip_sty(_style.tip);
            }
            else {
                *_output << _style.bg_sty(_style.bg);
            }
        }
        *_output << _style.end_sty(_style.end);
    }

    std::string
    progress_bar::format_percentage() const {
        std::stringstream ss;
        double const pct = progress() * 100;
        ss << std::setw(3) << std::floor(pct) << '%';
        return ss.str();
    }

    std::string
    progress_bar::format_ETA() const {
        if (_slowness_EST) {
            int s = static_cast<int>(std::floor((1.0 - progress()) * *_slowness_EST));

            int const h = s / 60 / 60;
            s %= 60 * 60;

            int const m = s / 60;
            s %= 60;

            std::stringstream ss;
            ss << "(ETA: " << std::setfill('0');
            if (h > 0) {
                ss << std::setw(2) << h << ':'
                   << std::setw(2) << m << ':'
                   << std::setw(2) << s;
            }
            else {
                ss << std::setw(2) << m << ':'
                   << std::setw(2) << s;
            }
            ss << ')';
            return ss.str();
        }
        else {
            // We have no data to estimate the remaining time.
            return std::string(std::strlen("(ETA: HH:MM)"), ' ');
        }
    }
}
