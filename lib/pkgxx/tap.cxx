#include <exception>
#include <iostream>

#include "tap.hxx"

namespace {
    struct bailed_out: public std::runtime_error {
        bailed_out(std::optional<std::string_view> const& reason = std::nullopt)
            : std::runtime_error("")
            , reason(reason
                     ? std::make_optional<std::string>(*reason)
                     : std::nullopt) {}

        std::optional<std::string> reason;
    };
}

namespace pkgxx {
    int
    tap::run(std::function<void (tap&)> const& fn) {
        try {
            fn(*this);
            if (_num_passed + _num_failed > 0) {
                std::cout << "1.." << _num_passed + _num_failed << std::endl;
            }
            return static_cast<int>(_num_failed);
        }
        catch (bailed_out const& e) {
            std::cout << "Bail out!";
            if (e.reason) {
                std::cout << ' ' << *e.reason;
            }
            std::cout << std::endl;
            return 255;
        }
        catch (std::exception const& e) {
            std::cout << "Bail out! " << e.what() << std::endl;
            return 255;
        }
        catch (...) {
            std::cout << "Bail out! An unknown exception was thrown." << std::endl;
            return 255;
        }
    }

    void
    tap::pass(tap::name_t const& name) {
        _num_passed++;
        std::cout << "ok " << _num_passed + _num_failed;
        if (name) {
            std::cout << ' ' << *name;
        }
        std::cout << std::endl;
    }

    void
    tap::fail(tap::name_t const& name) {
        _num_failed++;
        std::cout << "not ok " << _num_passed + _num_failed;
        if (name) {
            std::cout << ' ' << *name;
        }
        std::cout << std::endl;
    }

    void
    tap::diag(std::string_view const& msg) {
        for (std::string_view::size_type pos = 0; pos < msg.length(); ) {
            if (msg[pos] == '\n') {
                std::cout << '#' << std::endl;
                pos++;
            }
            else {
                if (auto const newline_pos = msg.find('\n', pos+1);
                    newline_pos != std::string_view::npos) {

                    std::cout << "# " << msg.substr(pos, newline_pos-pos-1) << std::endl;
                    pos = newline_pos+1;
                }
                else {
                    std::cout << "# " << msg.substr(pos) << std::endl;
                    break;
                }
            }
        }
    }

    [[noreturn]] void
    tap::BAIL_OUT(tap::reason_t const& reason) {
        throw bailed_out(reason);
    }
}
