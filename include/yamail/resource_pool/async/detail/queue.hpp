#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/time_traits.hpp>

#include <boost/asio/executor.hpp>
#include <boost/asio/post.hpp>

#include <algorithm>
#include <list>
#include <map>
#include <mutex>
#include <unordered_map>

namespace yamail {
namespace resource_pool {
namespace async {

namespace asio = boost::asio;

namespace detail {

using clock = std::chrono::steady_clock;

class expired_handler {
public:
    using executor_type = asio::executor;

    expired_handler() = default;

    template <class Handler>
    explicit expired_handler(Handler&& handler)
        : executor(asio::get_associated_executor(handler)),
          handler(std::forward<Handler>(handler)) {}

    void operator ()() {
        handler();
    }

    auto get_executor() const noexcept {
        return executor;
    }

private:
    asio::executor executor;
    std::function<void ()> handler;
};

template <class Value, class Mutex, class IoContext, class Timer>
class queue : public std::enable_shared_from_this<queue<Value, Mutex, IoContext, Timer>>,
    boost::noncopyable {
public:
    using value_type = Value;
    using io_context_t = IoContext;
    using timer_t = Timer;

    queue(std::size_t capacity) : _capacity(capacity) {}

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    bool empty() const;
    const timer_t& timer(io_context_t& io_context);

    template <class Handler>
    bool push(io_context_t& io_context, const value_type& req, Handler&& req_expired,
              time_traits::duration wait_duration);
    bool pop(io_context_t*& io_context, value_type& req);

private:
    using mutex_t = Mutex;
    using lock_guard = std::lock_guard<mutex_t>;

    struct expiring_request {
        using list = std::list<expiring_request>;
        using list_it = typename list::iterator;
        using multimap = std::multimap<time_traits::time_point, const expiring_request*>;
        using multimap_it = typename multimap::iterator;

        io_context_t* io_context;
        queue::value_type request;
        expired_handler expired;
        list_it order_it;
        multimap_it expires_at_it;

        expiring_request(io_context_t& io_context, const queue::value_type& request, expired_handler expired)
                : io_context(&io_context), request(request), expired(std::move(expired)) {}
    };

    using request_multimap_value = typename expiring_request::multimap::value_type;
    using timers_map = typename std::unordered_map<const io_context_t*, std::unique_ptr<timer_t>>;

    mutable mutex_t _mutex;
    typename expiring_request::list _ordered_requests;
    typename expiring_request::multimap _expires_at_requests;
    const std::size_t _capacity;
    timers_map _timers;
    time_traits::time_point _min_expires_at = time_traits::time_point::max();

    bool fit_capacity() const { return _expires_at_requests.size() < _capacity; }
    void cancel(const boost::system::error_code& ec, time_traits::time_point expires_at);
    void cancel_one(const request_multimap_value& pair);
    void update_timer();
    timer_t& get_timer(io_context_t& io_context);
};

template <class V, class M, class I, class T>
std::size_t queue<V, M, I, T>::size() const {
    const lock_guard lock(_mutex);
    return _expires_at_requests.size();
}

template <class V, class M, class I, class T>
bool queue<V, M, I, T>::empty() const {
    const lock_guard lock(_mutex);
    return _ordered_requests.empty();
}

template <class V, class M, class I, class T>
const typename queue<V, M, I, T>::timer_t& queue<V, M, I, T>::timer(io_context_t& io_context) {
    const lock_guard lock(_mutex);
    return get_timer(io_context);
}

template <class V, class M, class I, class T>
template <class Handler>
bool queue<V, M, I, T>::push(io_context_t& io_context, const value_type& req_data, Handler&& req_expired,
        time_traits::duration wait_duration) {
    const lock_guard lock(_mutex);
    if (!fit_capacity()) {
        return false;
    }
    auto order_it = _ordered_requests.insert(
        _ordered_requests.end(),
        expiring_request(io_context, req_data, expired_handler(std::forward<Handler>(req_expired)))
    );
    expiring_request& req = *order_it;
    req.order_it = order_it;
    const auto expires_at = time_traits::add(time_traits::now(), wait_duration);
    req.expires_at_it = _expires_at_requests.insert(std::make_pair(expires_at, &req));
    update_timer();
    return true;
}

template <class V, class M, class I, class T>
bool queue<V, M, I, T>::pop(io_context_t*& io_context, value_type& value) {
    const lock_guard lock(_mutex);
    if (_ordered_requests.empty()) {
        return false;
    }
    const expiring_request& req = _ordered_requests.front();
    io_context = req.io_context;
    value = req.request;
    _expires_at_requests.erase(req.expires_at_it);
    _ordered_requests.pop_front();
    update_timer();
    return true;
}

template <class V, class M, class I, class T>
void queue<V, M, I, T>::cancel(const boost::system::error_code& ec, time_traits::time_point expires_at) {
    if (ec) {
        return;
    }
    const lock_guard lock(_mutex);
    const auto begin = _expires_at_requests.begin();
    const auto end = _expires_at_requests.upper_bound(expires_at);
    std::for_each(begin, end, [&] (const request_multimap_value& v) { this->cancel_one(v); });
    _expires_at_requests.erase(_expires_at_requests.begin(), end);
    update_timer();
}

template <class V, class M, class I, class T>
void queue<V, M, I, T>::cancel_one(const request_multimap_value &pair) {
    const expiring_request* req = pair.second;
    asio::post(*req->io_context, std::move(req->expired));
    _ordered_requests.erase(req->order_it);
}

template <class V, class M, class I, class T>
void queue<V, M, I, T>::update_timer() {
    using timers_map_value = typename timers_map::value_type;
    if (_expires_at_requests.empty()) {
        std::for_each(_timers.begin(), _timers.end(), [] (timers_map_value& v) { v.second->cancel(); });
        _timers.clear();
        return;
    }
    const auto earliest_expire = _expires_at_requests.begin();
    const auto expires_at = earliest_expire->first;
    auto& timer = get_timer(*earliest_expire->second->io_context);
    timer.expires_at(expires_at);
    std::weak_ptr<queue> weak(this->shared_from_this());
    timer.async_wait([weak, expires_at] (const boost::system::error_code& ec) {
        if (const auto locked = weak.lock()) {
            locked->cancel(ec, expires_at);
        }
    });
}

template <class V, class M, class I, class T>
typename queue<V, M, I, T>::timer_t& queue<V, M, I, T>::get_timer(io_context_t& io_context) {
    auto it = _timers.find(&io_context);
    if (it != _timers.end()) {
        return *it->second;
    }
    return *_timers.insert(std::make_pair(&io_context, std::make_unique<timer_t>(io_context))).first->second;
}

} // namespace detail
} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP
