#pragma once

#include <future>
#include <set>
#include <tuple>
#include <vector>

#include <pkgxx/mutex_guard.hxx>
#include <pkgxx/pkgname.hxx>
#include <pkgxx/pkgpath.hxx>

namespace pkg_rr {
    /** Obtaining the set of installed packages having a specific flag is a
     * slow operation, but they can be combined with a relatively small
     * additional cost. With this class you can register many different
     * flags for aggregation and obtain resulting sets all at once. */
    struct package_scanner {
        /** A result of a single operation. */
        using result_type = std::map<pkgxx::pkgbase, pkgxx::pkgpath>;

        /** Construct an empty scanner that does nothing. */
        package_scanner(std::string const& PKG_INFO, unsigned concurrency)
            : _pkg_info(PKG_INFO)
            , _concurrency(concurrency) {}

        /** Destructing an instance of \c package_scanner causes all the
         * registered operations to run. */
        ~package_scanner() noexcept(false);

        /** Register a flag and a set of packages to exclude from the
         * result. The resulting future value will become available when
         * the instance of \c package_scanner gets destructed. */
        std::future<result_type>
        add_axis(
            std::string const& flag,
            std::set<pkgxx::pkgbase> const& exclude = {}) {

            auto& axis = _axes.emplace_back(
                std::promise<result_type>(),
                pkgxx::guarded<result_type>(),
                flag,
                exclude);
            return std::get<0>(axis).get_future();
        }

    private:
        std::string _pkg_info;
        unsigned _concurrency;
        std::vector<
            std::tuple<
                std::promise<result_type>,
                pkgxx::guarded<result_type>, // result
                std::string,                 // flag
                std::set<pkgxx::pkgbase>     // exclude
            >
        > _axes;
    };
}
