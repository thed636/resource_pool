#include <yamail/resource_pool/handle.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <boost/optional.hpp>

#include <list>

namespace {

using namespace testing;

struct handle_test : Test {};

struct resource {
    int value = 0;

    resource() = default;
    resource(const resource&) = delete;
    resource(resource&&) = default;
    resource& operator =(const resource &) = delete;
    resource& operator =(resource &&) = default;
};

struct idle {
    boost::optional<resource> value;
};

struct pool {
    struct pool_impl {
        typedef resource value_type;
        typedef std::list<idle>::iterator list_iterator;

        MOCK_METHOD1(waste, void (list_iterator));
    };

    typedef pool_impl::list_iterator list_iterator;
};

typedef yamail::resource_pool::handle<pool> resource_handle;

TEST(handle_test, construct_usable_should_be_not_unusable) {
    std::list<idle> resources;
    resources.emplace_back(idle {});
    const auto pool_impl = std::make_shared<pool::pool_impl>();
    const resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_FALSE(handle.unusable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_and_move_than_source_should_be_unusable) {
    std::list<idle> resources;
    resources.emplace_back(idle {});
    const auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle src(pool_impl, &resource_handle::waste, resources.begin());
    const resource_handle dst = std::move(src);
    EXPECT_TRUE(src.unusable());
    EXPECT_FALSE(dst.unusable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_and_move_over_assign_than_source_should_be_unusable) {
    std::list<idle> resources;
    resources.emplace_back(idle {});
    const auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle src(pool_impl, &resource_handle::waste, resources.begin());
    resource_handle dst(pool_impl, &resource_handle::waste, resources.end());
    dst = std::move(src);
    EXPECT_TRUE(src.unusable());
    EXPECT_FALSE(dst.unusable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_than_get_should_return_value) {
    std::list<idle> resources;
    resources.emplace_back(idle {resource {42}});
    auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_EQ(42, handle->value);
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_than_get_const_should_return_value) {
    std::list<idle> resources;
    resources.emplace_back(idle {resource {42}});
    auto pool_impl = std::make_shared<pool::pool_impl>();
    const resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_EQ(42, handle->value);
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());

}

TEST(handle_test, call_get_after_pool_impl_dtor_should_throw_exception) {
    std::list<idle> resources;
    resources.emplace_back(idle {resource {}});
    auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    pool_impl.reset();
    resources.clear();
    EXPECT_THROW(handle.get(), yamail::resource_pool::error::unusable_handle);
}

TEST(handle_test, call_get_const_after_pool_impl_dtor_should_throw_exception) {
    std::list<idle> resources;
    resources.emplace_back(idle {resource {}});
    auto pool_impl = std::make_shared<pool::pool_impl>();
    const resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    pool_impl.reset();
    resources.clear();
    EXPECT_THROW(handle.get(), yamail::resource_pool::error::unusable_handle);
}

TEST(handle_test, call_reset_after_pool_impl_dtor_should_throw_exception) {
    std::list<idle> resources;
    resources.emplace_back(idle {resource {}});
    auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    pool_impl.reset();
    resources.clear();
    EXPECT_THROW(handle.reset(resource {}), yamail::resource_pool::error::unusable_handle);
}

}
