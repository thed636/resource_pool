#include <yamail/resource_pool/error.hpp>

#include <gtest/gtest.h>

#include <limits>

namespace {

using namespace testing;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::error;

using boost::system::error_code;

struct error_test : Test {};

TEST(error_test, make_no_error_and_check_message) {
    const error_code error = make_error_code(code(ok));
    EXPECT_EQ(error.message(), "no error");
}

TEST(error_test, make_get_resource_timeout_error_and_check_message) {
    const error_code error = make_error_code(get_resource_timeout);
    EXPECT_EQ(error.message(), "get resource timeout");
}

TEST(error_test, make_request_queue_overflow_error_and_check_message) {
    const error_code error = make_error_code(request_queue_overflow);
    EXPECT_EQ(error.message(), "request queue overflow");
}

TEST(error_test, make_disabled_error_and_check_message) {
    const error_code error = make_error_code(disabled);
    EXPECT_EQ(error.message(), "resource pool is disabled");
}

TEST(error_test, make_client_handler_exception_error_and_check_message) {
    const error_code error = make_error_code(client_handler_exception);
    EXPECT_EQ(error.message(), "exception in client handler");
}

TEST(error_test, make_out_of_range_error_and_check_message) {
    const error_code error = make_error_code(code(std::numeric_limits<int>::max()));
    EXPECT_EQ(error.message(), "resource pool error");
}

TEST(error_test, make_no_error_and_category_name) {
    const error_code error = make_error_code(code(ok));
    EXPECT_EQ(error.category().name(), "yamail::resource_pool::error::detail::category");
}

}
