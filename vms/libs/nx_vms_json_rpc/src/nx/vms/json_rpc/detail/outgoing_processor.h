// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>
#include <variant>
#include <vector>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>
#include <nx/vms/api/data/json_rpc.h>

namespace nx::vms::json_rpc::detail {

/*
 * Class for internal usage by WebSocketConnection.
 */
class OutgoingProcessor
{
public:
    using Id = std::variant<QString, int>;
    using SendFunc = nx::utils::MoveOnlyFunc<void(QJsonValue)>;
    OutgoingProcessor(SendFunc sendFunc): m_sendFunc(std::move(sendFunc)) {}
    ~OutgoingProcessor() { clear(SystemError::interrupted); }

    void clear(SystemError::ErrorCode error);

    using ResponseHandler = nx::utils::MoveOnlyFunc<void(nx::vms::api::JsonRpcResponse)>;
    void processRequest(nx::vms::api::JsonRpcRequest jsonRpcRequest, ResponseHandler handler);

    using BatchResponseHandler =
        nx::utils::MoveOnlyFunc<void(std::vector<nx::vms::api::JsonRpcResponse>)>;
    void processBatchRequest(
        std::vector<nx::vms::api::JsonRpcRequest> jsonRpcRequests, BatchResponseHandler handler);

    void onResponse(const QJsonValue& data);

private:
    void send(nx::vms::api::JsonRpcRequest jsonRpcRequest) const;
    void send(std::vector<nx::vms::api::JsonRpcRequest> jsonRpcRequests) const;
    void onResponse(const QJsonArray& list);

private:
    using Key = size_t;
    struct AwaitingBatchResponse
    {
        std::vector<Id> ids;
        std::vector<nx::vms::api::JsonRpcResponse> errors;
        BatchResponseHandler handler;
    };

    SendFunc m_sendFunc;
    std::unordered_map<Id, ResponseHandler> m_awaitingResponses;
    Key m_nextBatchResponseKey = 0;
    std::unordered_map<Key, AwaitingBatchResponse> m_awaitingBatchResponseHolder;
    std::unordered_map<Id, Key> m_awaitingBatchResponses;
};

} // namespace nx::vms::json_rpc::detail
