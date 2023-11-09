// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/vms/json_rpc/detail/outgoing_processor.h>

namespace nx::vms::json_rpc::test {

TEST(OutgoingProcessor, batchRequest)
{
    std::vector<api::JsonRpcRequest> requests(2);
    requests[0].id = 1;
    requests[0].method = "method1";
    requests[1].id = "2";
    requests[1].method = "method2";
    std::vector<api::JsonRpcResponse> responses(2);
    responses[0].id = "2";
    responses[0].result = QJsonValue();
    responses[1].id = 1;
    responses[1].result = QJsonValue();
    QJsonValue responseJson;
    QJson::serialize(responses, &responseJson);

    detail::OutgoingProcessor processor(
        [](auto value)
        {
            ASSERT_EQ(value.toStdString(),
                "[{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"method1\"},"
                "{\"id\":\"2\",\"jsonrpc\":\"2.0\",\"method\":\"method2\"}]");
        });
    std::vector<api::JsonRpcResponse> expectedResponses;
    processor.processBatchRequest(
        requests, [&expectedResponses](auto value) { expectedResponses = std::move(value); });
    processor.onResponse(responseJson);
    ASSERT_EQ(
        QJson::serialized(expectedResponses[0]).toStdString(),
        QJson::serialized(responses[1]).toStdString());
    ASSERT_EQ(
        QJson::serialized(expectedResponses[1]).toStdString(),
        QJson::serialized(responses[0]).toStdString());
}

} // namespace nx::vms::json_rpc::test
