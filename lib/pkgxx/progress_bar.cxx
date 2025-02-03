#include "config.h"

#include <cmath>
#include <cstring>
#include <iomanip>
#include <signal.h>
#include <sstream>
#include <unistd.h>

#include "progress_bar.hxx"
#include "scoped_signal_handler.hxx"
#include "tty.hxx"

namespace pkgxx {
    progress_bar::progress_bar(
        int,
        std::size_t total,
        std::optional<std::reference_wrapper<std::ostream>> const& output,
        double decay_p,
        bool show_percent,
        bool show_ETA,
        bar_style const& style,
        std::chrono::steady_clock::duration const& redraw_rate)
        : _output(output ? value_or_ref(output->get()) : default_output())
        , _decay_p(decay_p)
        , _show_percent(show_percent)
        , _show_ETA(show_ETA)
        , _style(std::move(style))
        , _redraw_rate(redraw_rate)
        , _term_size(ttyp() ? ttyp()->size() : std::nullopt)
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
            _winch_handler = std::make_unique<scoped_signal_handler>(
                std::initializer_list<int> {SIGWINCH},
                [this]() {
                    lock_t lk(_mtx);
                    _term_size = tty().size();
                    redraw(true);
                });
        }
#endif
        redraw();
    }

    progress_bar::~progress_bar() {
        // We must destroy the SIGWINCH handler first, otherwise there can
        // be a race condition.
        _winch_handler.reset();

        if (should_draw()) {
            tty() << tty::move_x(0)
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

    value_or_ref<std::ostream>
    progress_bar::default_output() {
        if (auto const fd = STDERR_FILENO; cisatty(fd)) {
            return value_or_ref<std::ostream>(
                std::in_place_type_t<ttystream>(), fd, "owned"_na = false);
        }
        else {
            return value_or_ref<std::ostream>(
                std::in_place_type_t<fdostream>(), fd, false);
        }
    }

    void
    progress_bar::redraw(bool force) {
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
                tty() << tty::move_x(0)
                      << tty::erase_line_from_cursor
                      << std::flush;
                return;
            }
            else {
                bar_width -= 1 + elem.length();
            }
        }

        std::string const bar = format_bar(bar_width);
        tty() << tty::move_x(0)
              << bar; // No need to erase the line. We can just overwrite it.
        for (auto const& elem: postfix) {
            tty() << ' ' << elem;
        }
        tty() << std::flush;
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

    std::string
    progress_bar::format_bar(std::size_t width) const {
        if (width < 2) {
            // The terminal is too narrow.
            return "";
        }

        std::size_t const prog =
            static_cast<std::size_t>(
                std::floor(progress() * static_cast<double>(width - 2)));

        std::string bar;
        bar.resize(width);
        bar.push_back(_style.begin);
        for (std::size_t i = 0; i < width - 2; i++) {
            if (i < prog) {
                bar.push_back(_style.fill);
            }
            else if (i == prog) {
                bar.push_back(_style.tip);
            }
            else {
                bar.push_back(_style.bg);
            }
        }
        bar.push_back(_style.end);
        return bar;
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
