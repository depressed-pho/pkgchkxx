#pragma once

#include <pkgxx/config.h>

#include <cassert>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <signal.h>
#include <type_traits>
#include <variant>

#include <pkgxx/ordered.hxx>

namespace pkgxx {
    /** A wrapper for sigset_t */
    struct csigset {
        struct iterator : public equality_comparable<iterator> {
            using iterator_category = std::bidirectional_iterator_tag;
            /// Difference doesn't make sense for this iterator but we
            /// still need to define it, otherwise std::iterator_traits
            /// won't recognize this as an iterator.
            using difference_type   = void;
            using value_type        = int;
            using pointer           = value_type*;
            using reference         = value_type&;

            bool
            operator== (iterator const& other) const noexcept;

            reference
            operator* ();

            pointer
            operator-> ();

            iterator&
            operator++ ();

            iterator
            operator++ (int) {
                auto it = *this;
                ++(*this);
                return it;
            }

            iterator&
            operator-- ();

            iterator
            operator-- (int) {
                auto it = *this;
                ++(*this);
                return it;
            }

        private:
            friend struct csigset;

            iterator()
                : _ref(nullptr) {}

            iterator(csigset const& ref);
            iterator(csigset const& ref, int const cur);

        private:
            csigset const* _ref;
            // The position of the iterator; a valid signo or nullopt.
            std::optional<int> _cur;
        };

        enum how {
            block   = SIG_BLOCK,
            unblock = SIG_UNBLOCK,
            setmask = SIG_SETMASK
        };

        using key_type       = int;
        using value_type     = int;
        using size_type      = std::size_t;
        using const_iterator = iterator const;

        /** Construct an empty signal set. */
        csigset();

        /** Copy a signal set. */
        csigset(csigset const& set);

        /** Reference an existing non-const sigset_t.
         */
        explicit csigset(sigset_t& ref)
            : _set(std::ref(ref)) {}

        /** Copy an existing const sigset_t. */
        explicit csigset(sigset_t const& ref);

        /** Obtain the current process-wide signal mask. */
        static csigset
        procmask();

        /** Manipulate the process-wide signal mask. */
        static csigset
        procmask(how const& how_, csigset const& set);

        csigset&
        operator= (csigset const& set);

        csigset&
        operator= (sigset_t const& ref);

        void
        clear();

        std::pair<iterator, bool>
        insert(int const signo);

        iterator
        insert(const_iterator, int const signo) {
            return insert(signo).first;
        }

        bool
        contains(int const signo) const {
            return sigismember(get(), signo);
        }

        iterator
        begin() { return iterator(*this); }

        const_iterator
        begin() const { return iterator(*this); }

        const_iterator
        cbegin() const { return iterator(*this); }

        iterator
        end() { return iterator(); }

        const_iterator
        end() const { return iterator(); }

        const_iterator
        cend() const { return iterator(); }

        /** Obtain the raw pointer to the underlying sigset_t. */
        sigset_t const*
        get() const;

        /** Obtain the raw pointer to the underlying sigset_t. */
        sigset_t*
        get();

    private:
        std::variant<
            sigset_t, // owned sigset_t
            std::reference_wrapper<sigset_t>
            > _set;
    };

    /** A wrapper for struct sigaction */
    struct csigaction {
        using sigaction_t = void(*)(int sig, siginfo_t* info, void* ctx);
        using handler_t   = void(*)(int sig);

        template <bool is_const>
        struct handler_wrapper {
            using parent_type = std::conditional_t<
                                    is_const,
                                    csigaction const,
                                    csigaction>;

            /** Assign an extended signal handler to a sigaction. Only
             * available when the sigaction is non-const.
             */
            template <bool is_const_ = is_const,
                      typename = std::enable_if_t<!is_const_>>
            handler_wrapper<is_const_>&
            operator= (sigaction_t const action) {
                _ref._sa.sa_sigaction = action;
                _ref._sa.sa_flags |= SA_SIGINFO;
                return *this;
            }

            /** Assign a regular signal handler to a sigaction. Only
             * available when the sigaction is non-const.
             */
            template <bool is_const_ = is_const,
                      typename = std::enable_if_t<!is_const_>>
            handler_wrapper<is_const_>&
            operator= (handler_t const handler) {
                _ref._sa.sa_handler = handler;
                _ref._sa.sa_flags &= ~SA_SIGINFO;
                return *this;
            }

            // No support for getting handlers atm.

        private:
            friend struct csigaction;

            handler_wrapper(parent_type& ref)
                : _ref(ref) {}

        private:
            parent_type& _ref;
        };

        /** Construct a default sigaction. */
        csigaction();

        /** Copy a sigaction. */
        csigaction(csigaction const& sa);

        /** Obtain the current sigaction. */
        static csigaction
        current(int const signo);

        /** Install a sigaction and return the previous one. */
        csigaction
        install(int const signo) const;

        handler_wrapper<false>
        handler() {
            return handler_wrapper<false>(*this);
        }

        /** Return a \ref csigset referencing the mask for this
         * action. Mutating the resulting \ref csigset affects this \ref
         * csigaction.
         */
        csigset
        mask() {
            return csigset(_sa.sa_mask);
        }

        // No support for manipulating flags (other than SA_SIGINFO) atm.

        /** Obtain the raw pointer to the underlying struct sigaction. */
        struct sigaction const*
        get() const {
            return &_sa;
        }

        /** Obtain the raw pointer to the underlying struct sigaction. */
        struct sigaction*
        get() {
            return &_sa;
        }

    private:
        struct sigaction _sa;
    };

    /** A polymorphic wrapper for siginfo_t. */
    struct csiginfo_base {
        csiginfo_base(siginfo_t const& si)
            : signo(si.si_signo)
            , code(si.si_code) {}

        virtual ~csiginfo_base() {}

        int signo;
        int code;
    };
    struct csiginfo_queued: public csiginfo_base {
        csiginfo_queued(siginfo_t const& si)
            : csiginfo_base(si)
            , pid(si.si_pid)
            , uid(si.si_uid)
            , value(si.si_value) {

            assert(si.si_code == SI_QUEUE);
        }

        virtual ~csiginfo_queued() {}

        pid_t pid;
        uid_t uid;
        sigval value;
    };

    constexpr bool
    is_sigwaitinfo_available() {
#if defined(HAVE_SIGWAITINFO)
        return true;
#else
        return false;
#endif
    }

    constexpr bool
    is_sigqueue_available() {
#if defined(HAVE_SIGQUEUE)
        return true;
#else
        return false;
#endif
    }

    /** A wrapper for sigwaitinfo(2). If the system lacks this syscall,
     * this function emulates it via sigwait(2) and always returns \ref
     * csiginfo_base with only \ref csiginfo_base::signo being filled.
     */
    std::unique_ptr<csiginfo_base>
    csigwaitinfo(csigset const& set);

    /** A wrapper for sigqueue(2). If the system lacks this syscall, this
     * function emulates it via kill(2), ignoring \c int_value.
     */
    void
    csigqueue(pid_t const pid, int const signo, int const int_value);

    /** A wrapper for sigqueue(2). If the system lacks this syscall, this
     * function emulates it via kill(2), ignoring \c ptr_value.
     */
    void
    csigqueue(pid_t const pid, int const signo, void* const ptr_value);
}
