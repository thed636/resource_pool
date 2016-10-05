#include <yamail/resource_pool/async/pool.hpp>

#include <fstream>
#include <future>
#include <thread>

typedef yamail::resource_pool::async::pool<std::ofstream> ofstream_pool;
typedef yamail::resource_pool::time_traits time_traits;

struct on_get {
    void operator ()(const boost::system::error_code& ec, ofstream_pool::handle handle) {
        if (ec) {
            std::cerr << ec.message() << std::endl;
            return;
        }
        if (handle.empty()) {
            std::ofstream file;
            try {
                file = std::ofstream("pool.log", std::ios::app);
            } catch (const std::exception& exception) {
                std::cerr << "Open file pool.log error: " << exception.what() << std::endl;
                return;
            }
            handle.reset(std::move(file));
        }
        handle.get() << (time_traits::time_point::min() - time_traits::now()).count() << std::endl;
        handle.recycle();
    }
};

int main() {
    boost::asio::io_service service;
    boost::asio::io_service::work work(service);
    std::thread worker([&] { return service.run(); });
    ofstream_pool pool(service, 1, 10);
    on_get handler;
    std::promise<void> promise;
    auto future = promise.get_future();
    struct auto_set_value {
        std::promise<void>& promise;
        ~auto_set_value() { promise.set_value(); }
    };
    const auto sync_wrap = [&] (const boost::system::error_code& ec, ofstream_pool::handle handle) {
        auto_set_value set_value {promise};
        handler(ec, std::move(handle));
    };
    pool.get_auto_waste(sync_wrap, time_traits::duration::max());
    future.wait();
    service.stop();
    worker.join();
    return 0;
}
