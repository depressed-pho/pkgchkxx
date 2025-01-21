#include "config.h"

#include <exception>
#include <system_error>
#include <type_traits>
#include <unistd.h>

#include "signal.hxx"

namespace {
    inline int
    nsig() {
#if defined(HAVE_SYSCONF) && HAVE_DECL__SC_NSIG
        // _SC_NSIG is a POSIX 2024 thing. It's an ideal way to determine
        // the upper bound of signal numbers, but not all systems have it.
        return sysconf(_SC_NSIG);

#elif defined(NSIG)
        // Non-standard but common.
        return NSIG;

#elif defined(_NSIG)
        // Non-standard. Apparently common among BSD.
        return _NSIG;

#else
        // The last resort. Not guaranteed to work.
        return 64;
#endif
    }
}

namespace pkgxx {
    bool
    csigset::iterator::operator== (iterator const& other) const noexcept {
        if (_cur.has_value()) {
            return _ref == other._ref && _cur == other._cur;
        }
        else {
            return !other._cur.has_value();
        }
    }

    csigset::iterator::reference
    csigset::iterator::operator* () {
        return _cur.value();
    }

    csigset::iterator::pointer
    csigset::iterator::operator-> () {
        return _cur.operator->();
    }

    csigset::iterator&
    csigset::iterator::operator++ () {
        if (_cur) {
            assert(_ref);
            (*_cur)++;
            for (; *_cur < nsig(); (*_cur)++) {
                if (_ref->contains(*_cur)) {
                    return *this;
                }
            }
            // We have reached the end of the ordered set.
            _cur.reset();
        }
        return *this;
    }

    csigset::iterator&
    csigset::iterator::operator-- () {
        if (_cur) {
            assert(_ref);
            (*_cur)--;
            for (; *_cur >= 1; (*_cur)--) {
                if (_ref->contains(*_cur)) {
                    return *this;
                }
            }
            // We have reached the beginning of the ordered set.
            _cur.reset();
        }
        return *this;
    }

    csigset::iterator::iterator(csigset const& ref)
        : _ref(&ref) {

        for (int signo = 1; signo < nsig(); signo++) {
            if (ref.contains(signo)) {
                _cur = signo;
                break;
            }
        }
    }

    csigset::iterator::iterator(csigset const& ref, int const cur)
        : _ref(&ref)
        , _cur(cur) {

        assert(ref.contains(cur));
    }

    csigset::csigset() {
        if (sigemptyset(get()) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigemptyset");
        }
    }

    csigset::csigset(csigset const& set) {
        *this = set;
    }

    csigset::csigset(sigset_t const& ref) {
        *this = ref;
    }

    csigset
    csigset::procmask() {
        csigset tmp;
        if (sigprocmask(0, nullptr, tmp.get()) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigprocmask");
        }
        return tmp;
    }

    csigset
    csigset::procmask(how const& how_, csigset const& set) {
        csigset tmp;
        if (sigprocmask(static_cast<int>(how_), set.get(), tmp.get()) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigprocmask");
        }
        return tmp;
    }

    csigset&
    csigset::operator= (csigset const& set) {
        // POSIX doesn't say nor imply whether cloning a sigset_t just by
        // an assignment is safe or not. This is obviously inefficient but
        // we must play safe.
        clear();
        for (int signo = 1; signo < nsig(); signo++) {
            if (set.contains(signo)) {
                insert(signo);
            }
        }
        return *this;
    }

    csigset&
    csigset::operator= (sigset_t const& ref) {
        clear();
        for (int signo = 1; signo < nsig(); signo++) {
            if (sigismember(&ref, signo)) {
                insert(signo);
            }
        }
        return *this;
    }

    void
    csigset::clear() {
        if (sigemptyset(get()) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigemptyset");
        }
    }

    std::pair<csigset::iterator, bool>
    csigset::insert(int const signo) {
        bool const inserted = !sigismember(get(), signo);
        if (sigaddset(get(), signo) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigaddset");
        }
        return std::make_pair(iterator(*this, signo), inserted);
    }

    sigset_t const*
    csigset::get() const {
        return std::visit([](auto&& arg) -> sigset_t const* {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, sigset_t>) {
                return &arg;
            }
            else {
                static_assert(std::is_same_v<T, std::reference_wrapper<sigset_t>>);
                return &arg.get();
            }
        }, _set);
    }

    sigset_t*
    csigset::get() {
        return std::visit([](auto&& arg) -> sigset_t* {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, sigset_t>) {
                return &arg;
            }
            else {
                static_assert(std::is_same_v<T, std::reference_wrapper<sigset_t>>);
                return &arg.get();
            }
        }, _set);
    }

    csigaction::csigaction() {
        _sa.sa_handler = SIG_DFL;
        _sa.sa_flags   = 0;
        if (sigemptyset(&_sa.sa_mask) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigemptyset");
        }
    }

    csigaction::csigaction(csigaction const& sa) {
        if ((sa._sa.sa_flags & SA_SIGINFO) != 0) {
            _sa.sa_sigaction = sa._sa.sa_sigaction;
        }
        else {
            _sa.sa_handler = sa._sa.sa_handler;
        }
        mask() = sa._sa.sa_mask;
        _sa.sa_flags = sa._sa.sa_flags;
    }

    csigaction
    csigaction::current(int const signo) {
        csigaction tmp;
        if (sigaction(signo, nullptr, tmp.get()) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigaction");
        }
        return tmp;
    }

    csigaction
    csigaction::install(int const signo) const {
        csigaction tmp;
        if (sigaction(signo, get(), tmp.get()) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigaction");
        }
        return tmp;
    }

    std::unique_ptr<csiginfo_base>
    csigwaitinfo(csigset const& set) {
        siginfo_t si;

        if (sigwaitinfo(set.get(), &si) == -1) {
            // -1 is the only error it returns. On success it returns a
            // -signal number.
            throw std::system_error(errno, std::generic_category(), "sigwaitinfo");
        }

        switch (si.si_code) {
        case SI_QUEUE:
            return std::make_unique<csiginfo_queued>(si);

        default:
            // Sorry but we don't know about this code.
            return std::make_unique<csiginfo_base>(si);
        }
    }

    void
    csigqueue(pid_t const pid, int const signo, int const int_value) {
        sigval sv;
        sv.sival_int = int_value;
        if (sigqueue(pid, signo, sv) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigqueue");
        }
    }

    void
    csigqueue(pid_t const pid, int const signo, void* const ptr_value) {
        sigval sv;
        sv.sival_ptr = ptr_value;
        if (sigqueue(pid, signo, sv) != 0) {
            throw std::system_error(errno, std::generic_category(), "sigqueue");
        }
    }
}
