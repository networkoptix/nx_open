// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/json_rpc/detail/outgoing_processor.h>
#include <nx/utils/json/qjson.h>

namespace nx::json_rpc::test {

TEST(OutgoingProcessor, batchRequest)
{
    std::vector<Request> requests;
    requests.emplace_back(Request::create(1, "method1", QJsonObject{}));
    requests.emplace_back(Request::create("2", "method2", QJsonArray{}));
    std::vector<Response> responses;
    responses.emplace_back(Response::makeResult("2", QJsonArray{}));
    responses.emplace_back(Response::makeResult(1, QJsonObject{}));
    auto responseJson{nx::json::serialized(responses, /*stripDefault*/ false)};

    detail::OutgoingProcessor processor(
        [](auto value)
        {
            ASSERT_EQ(value,
                "[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"method1\",\"params\":{}},"
                "{\"jsonrpc\":\"2.0\",\"id\":\"2\",\"method\":\"method2\",\"params\":[]}]");
        });
    std::vector<Response> expectedResponses;
    processor.processBatchRequest(std::move(requests),
        [&expectedResponses](auto value) { expectedResponses = std::move(value); });
    processor.onResponse(std::move(responseJson));
    ASSERT_EQ(
        nx::reflect::json::serialize(expectedResponses[0]),
        nx::reflect::json::serialize(responses[1]));
    ASSERT_EQ(
        nx::reflect::json::serialize(expectedResponses[1]),
        nx::reflect::json::serialize(responses[0]));
}

} // namespace nx::json_rpc::test
