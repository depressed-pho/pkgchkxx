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
        is(T0&& got, T1&& expected, name_t const& name = std::nullopt);

        [[noreturn]] void
        BAIL_OUT(reason_t const& reason = std::nullopt);

    private:
        unsigned _num_passed;
        unsigned _num_failed;
    };

    // Implementation

    template <typename T0, typename T1>
    void
    tap::is(T0&& got, T1&& expected, name_t const& name) {
        if (got == expected) {
            pass(name);
        }
        else {
            fail(name);

            std::stringstream ss;
            ss << "Expected " << expected << " but got " << got;
            diag(ss.str());
        }
    }
}
