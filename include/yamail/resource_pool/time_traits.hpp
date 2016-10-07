#ifndef YAMAIL_RESOURCE_POOL_TIME_TRAITS_HPP
#define YAMAIL_RESOURCE_POOL_TIME_TRAITS_HPP

#include <boost/asio/basic_waitable_timer.hpp>

#include <chrono>

namespace yamail {
namespace resource_pool {

struct time_traits {
    typedef std::chrono::steady_clock::duration duration;
    typedef std::chrono::steady_clock::time_point time_point;
    typedef boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer;

    static time_point now() {
        return std::chrono::steady_clock::now();
    }

    static time_point add(time_point t, duration d) {
        const time_point epoch;
        if (t >= epoch) {
            if (time_point::max() - t < d) {
                return time_point::max();
            }
        } else {
            if (-(t - time_point::min()) > d) {
                return time_point::min();
            }
        }
        return t + d;
    }
};

} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_TIME_TRAITS_HPP
