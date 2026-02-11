// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>
#include <vector>

#include <nx/utils/move_only_func.h>

#include "messages.h"
#include "websocket_connection_stats.h"

namespace nx::json_rpc {

class NX_JSON_RPC_API IncomingProcessor
{
public:
    using ResponseHandler = nx::MoveOnlyFunc<void(Response)>;
    using RequestHandler = nx::MoveOnlyFunc<void(Request, ResponseHandler)>;

    IncomingProcessor(RequestHandler handler = {}): m_handler(std::move(handler)) {}
    void processRequest(
        rapidjson::Document data, nx::MoveOnlyFunc<void(std::string)> handler);

    WebSocketConnectionMessages messages() const { return m_messages; }

private:
    struct BatchRequest
    {
        std::vector<bool> requests;
        std::vector<Response> responses;
        nx::MoveOnlyFunc<void(std::string)> handler;
    };

    void onBatchResponse(BatchRequest* batchRequest, int request, Response response);

    void processBatchRequest(
        rapidjson::Document list, nx::MoveOnlyFunc<void(std::string)> handler);

    void sendResponse(
        Response response, const nx::MoveOnlyFunc<void(std::string)>& handler);

private:
    RequestHandler m_handler;
    std::unordered_map<BatchRequest*, std::unique_ptr<BatchRequest>> m_batchRequests;
    WebSocketConnectionMessages m_messages;
};

} // namespace nx::json_rpc::detail
