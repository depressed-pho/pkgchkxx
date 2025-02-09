#pragma once

#include <iostream>
#include <optional>

#include <pkgxx/tty.hxx>

namespace pkg_rr {
    /** A variant of \ref pkgxx::maybe_tty_syncbuf but modifies outputs.
     */
    struct msgstreambuf: public virtual pkgxx::ttystreambuf_base
                       , public virtual std::streambuf {
        msgstreambuf(
            pkgxx::ttystreambuf_base& out,
            pkgxx::tty::style const& default_style = {});

        virtual ~msgstreambuf();

        // Don't call this.
        [[noreturn]] virtual std::lock_guard<std::mutex>
        lock() override;

        virtual std::optional<pkgxx::dimension<std::size_t>>
        term_size() const override;

        virtual void
        push_style(pkgxx::tty::style const& sty, pkgxx::ttystream_base::how how_) override;

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
        enum class state {
            initial, // The next line sould follow "RR> "
            newline, // The next line should follow "rr> "
            general
        };

        void
        print_prefix();

    private:
        pkgxx::maybe_tty_syncbuf _out;
        state _state;
        pkgxx::tty::style _default_sty;
        pkgxx::tty::style _RR_sty;
        pkgxx::tty::style _rr_sty;
    };

    /** A variant of \ref pkgxx::maybe_tty_osyncstream but modifies outputs.
     */
    struct msgstream: public virtual pkgxx::ttystream_base
                    , public virtual std::ostream {

        msgstream();
        msgstream(ttystream_base& out, pkgxx::tty::style const& default_style = {});
        msgstream(msgstream&& other);
        virtual ~msgstream() = default;

        virtual std::optional<pkgxx::dimension<std::size_t>>
        size() const override;

        virtual void
        push_style(pkgxx::tty::style const& sty, how how_) override;

        virtual void
        pop_style() override;

    private:
        std::unique_ptr<msgstreambuf> _buf;
    };

    template <typename T>
    msgstream&
    operator<< (msgstream& out, T const& val) {
        static_cast<pkgxx::ttystream_base&>(out) << val;
        return out;
    }

    template <typename T>
    msgstream&&
    operator<< (msgstream&& out, T const& val) {
        static_cast<pkgxx::ttystream_base&>(out) << val;
        return std::move(out);
    }
}
