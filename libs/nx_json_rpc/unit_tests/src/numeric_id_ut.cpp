// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <limits>

#include <gtest/gtest.h>

#include <nx/json_rpc/messages.h>

namespace nx::json_rpc::test {

TEST(NumericId, Int64)
{
    ASSERT_EQ(
        nx::reflect::json::serialize(
            Request::create(std::numeric_limits<int64_t>::max(), "method")),
        "{\"jsonrpc\":\"2.0\",\"id\":9223372036854775807,\"method\":\"method\"}");
    ASSERT_EQ(
        nx::reflect::json::serialize(
            Request::create(std::numeric_limits<int64_t>::min(), "method")),
        "{\"jsonrpc\":\"2.0\",\"id\":-9223372036854775808,\"method\":\"method\"}");

    Request request{rapidjson::Document::AllocatorType{}};
    ASSERT_EQ(
        nx::reflect::json::deserialize(
            "{\"jsonrpc\":\"2.0\",\"id\":9223372036854775808,\"method\":\"method\"}", &request),
        nx::reflect::DeserializationResult(
            false, "Signed 64 bit integer expected", "9223372036854775808"));
}

} // namespace nx::json_rpc::test
