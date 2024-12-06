// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>
#include <variant>
#include <vector>

#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

#include "../messages.h"

namespace nx::json_rpc::detail {

/*
 * Class for internal usage by WebSocketConnection.
 */
class OutgoingProcessor
{
public:
    using Id = std::variant<QString, int>;
    using SendFunc = nx::utils::MoveOnlyFunc<void(std::string)>;
    OutgoingProcessor(SendFunc sendFunc): m_sendFunc(std::move(sendFunc)) {}
    ~OutgoingProcessor() { clear(SystemError::interrupted); }

    void clear(SystemError::ErrorCode error);

    using ResponseHandler = nx::utils::MoveOnlyFunc<void(Response)>;
    void processRequest(Request request, ResponseHandler handler, std::string serializedRequest);

    using BatchResponseHandler = nx::utils::MoveOnlyFunc<void(std::vector<Response>)>;
    void processBatchRequest(std::vector<Request> jsonRpcRequests, BatchResponseHandler handler);

    void onResponse(rapidjson::Document data);

private:
    void send(Request request, std::string serializedRequest) const;
    void send(std::vector<Request> jsonRpcRequests) const;
    void onArrayResponse(rapidjson::Document list);

private:
    using Key = size_t;
    struct AwaitingBatchResponse
    {
        std::vector<Id> ids;
        std::vector<Response> errors;
        BatchResponseHandler handler;
    };

    SendFunc m_sendFunc;
    std::unordered_map<Id, ResponseHandler> m_awaitingResponses;
    Key m_nextBatchResponseKey = 0;
    std::unordered_map<Key, AwaitingBatchResponse> m_awaitingBatchResponseHolder;
    std::unordered_map<Id, Key> m_awaitingBatchResponses;
};

} // namespace nx::json_rpc::detail
