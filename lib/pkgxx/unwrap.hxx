#pragma once

#include <set>
#include <type_traits>

namespace pkgxx {
    /// std::unwrap_reference is a C++20 thing and we can't use it atm.
    template <typename T>
    struct unwrap_reference {
        using type = T;
    };
    template <typename T>
    struct unwrap_reference<std::reference_wrapper<T>> {
        using type = T&;
    };

    /// std::unwrap_reference_t is a C++20 thing and we can't use it atm.
    template <typename T>
    using unwrap_reference_t = typename unwrap_reference<T>::type;

    /// std::unwrap_ref_decay is a C++20 thing and we can't use it atm.
    template <typename T>
    struct unwrap_ref_decay: unwrap_reference<std::decay_t<T>> {};

    /// std::unwrap_ref_decay_t is a C++20 thing and we can't use it atm.
    template <typename T>
    using unwrap_ref_decay_t = typename unwrap_ref_decay<T>::type;

    /// Compare two sets std::set<T1> and std::set<T2> where T1 and T2 are
    /// equivalent under std::unwrap_ref_decay. Dunno if this is a good
    /// idea.
    template <typename T1, typename C1, typename A1,
              typename T2, typename C2, typename A2>
    [[gnu::pure]] inline
    std::enable_if_t<
        std::conjunction_v<
            std::is_same<
                std::decay_t<pkgxx::unwrap_reference_t<T1>>,
                std::decay_t<pkgxx::unwrap_reference_t<T2>>
                >,
            std::is_same<C1, C2>
            >,
        bool>
    operator== (std::set<T1, C1, A1> const& a, std::set<T2, C2, A2> const& b) {
        if (a.size() == b.size()) {
            for (auto ia = a.begin(), ib = b.begin();
                 ia != a.end();
                 ia++, ib++) {

                auto const& ra = static_cast<std::decay_t<pkgxx::unwrap_ref_decay_t<T1>> const&>(*ia);
                auto const& rb = static_cast<std::decay_t<pkgxx::unwrap_ref_decay_t<T2>> const&>(*ib);
                if (ra != rb) {
                    return false;
                }
            }
            return true;
        }
        else {
            return false;
        }
    }

    /// Compare two sets std::set<T1> and std::set<T2> where T1 and T2 are
    /// equivalent under std::unwrap_ref_decay. Dunno if this is a good
    /// idea.
    template <typename T1, typename C1, typename A1,
              typename T2, typename C2, typename A2>
    [[gnu::pure]] inline
    std::enable_if_t<
        std::conjunction_v<
            std::is_same<
                std::decay_t<pkgxx::unwrap_reference_t<T1>>,
                std::decay_t<pkgxx::unwrap_reference_t<T2>>
                >,
            std::is_same<C1, C2>
            >,
        bool>
    operator!= (std::set<T1, C1, A1> const& a, std::set<T2, C2, A2> const& b) {
        return !(a == b);
    }
}
