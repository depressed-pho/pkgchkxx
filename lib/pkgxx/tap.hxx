#include <functional>
#include <optional>
#include <sstream>
#include <string_view>

namespace pkgxx {
    /** An implementation of TAP producer -- Test Anything Protocol.
     */
    struct tap {
        using name_t   = std::optional<std::string_view>;
        using reason_t = std::optional<std::string_view>;

        tap()
            : _num_passed(0)
            , _num_failed(0) {}

        /** Invoke a function with an exception guard. When an exception is
         * thrown from the function, it emits a "Bail out!" directive. The
         * returned value is a preferred status code for exitting.
         */
        int
        run(std::function<void (tap&)> const& fn);

        // FIXME: We seriously want to use std::source_location here but
        // it's a C++20 thing.
        void
        pass(name_t const& name = std::nullopt);

        void
        fail(name_t const& name = std::nullopt);

        void
        diag(std::string_view const& msg);

        template <typename T0, typename T1>
        void
        is(T0 const& got, T1 const& expected, name_t const& name = std::nullopt);

        template <typename T0, typename T1>
        void
        lt(T0 const& got, T1 const& expected, name_t const& name = std::nullopt);

        [[noreturn]] void
        BAIL_OUT(reason_t const& reason = std::nullopt);

    private:
        unsigned _num_passed;
        unsigned _num_failed;
    };

    // Implementation

    namespace detail {
        inline constexpr int const MAX_SHOW_LENGTH = 256;

        template <typename T>
        std::string
        show(const T& val) {
            std::stringstream ss;
            ss << val;
            auto const str = ss.str();

            if (str.length() <= MAX_SHOW_LENGTH) {
                return str;
            }
            else {
                std::stringstream ss;
                ss << str.length() << " bytes of string";
                return ss.str();
            }
        }
    }

    template <typename T0, typename T1>
    void
    tap::is(T0 const& got, T1 const& expected, name_t const& name) {
        if (got == expected) {
            pass(name);
        }
        else {
            fail(name);

            std::stringstream ss;
            ss << "Expected " << detail::show(expected)
               << " but got " << detail::show(got);
            diag(ss.str());
        }
    }

    template <typename T0, typename T1>
    void
    tap::lt(T0 const& got, T1 const& expected, name_t const& name) {
        if (got < expected) {
            pass(name);
        }
        else {
            fail(name);

            std::stringstream ss;
            ss << "Expected "         << detail::show(expected)
               << " to be less than " << detail::show(got);
            diag(ss.str());
        }
    }
}
