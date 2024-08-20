// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>
#include <vector>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

#include <nx/utils/move_only_func.h>

#include "messages.h"

namespace nx::vms::json_rpc {

class NX_VMS_JSON_RPC_API IncomingProcessor
{
public:
    using ResponseHandler = nx::utils::MoveOnlyFunc<void(JsonRpcResponse)>;
    using RequestHandler = nx::utils::MoveOnlyFunc<void(JsonRpcRequest, ResponseHandler)>;

    IncomingProcessor(RequestHandler handler = {}): m_handler(std::move(handler)) {}
    void processRequest(const QJsonValue& data, nx::utils::MoveOnlyFunc<void(QJsonValue)> handler);

private:
    struct Request
    {
        JsonRpcRequest jsonRpcRequest;

        Request(JsonRpcRequest jsonRpcRequest):
            jsonRpcRequest(std::move(jsonRpcRequest))
        {
        }
    };

    struct BatchRequest
    {
        std::unordered_map<Request*, std::unique_ptr<Request>> requests;
        std::vector<JsonRpcResponse> responses;
        nx::utils::MoveOnlyFunc<void(QJsonValue)> handler;
    };

    void onBatchResponse(
        BatchRequest* batchRequest, Request* request, JsonRpcResponse response);

    void processBatchRequest(
        const QJsonArray& list, nx::utils::MoveOnlyFunc<void(QJsonValue)> handler);

    void sendResponse(JsonRpcResponse jsonRpcResponse,
        const nx::utils::MoveOnlyFunc<void(QJsonValue)>& handler);

private:
    RequestHandler m_handler;
    std::unordered_map<BatchRequest*, std::unique_ptr<BatchRequest>> m_batchRequests;
    std::unordered_map<Request*, std::unique_ptr<Request>> m_requests;
};

} // namespace nx::vms::json_rpc::detail
