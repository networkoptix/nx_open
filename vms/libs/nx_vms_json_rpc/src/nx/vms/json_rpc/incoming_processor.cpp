// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "incoming_processor.h"

#include <nx/fusion/serialization/json_functions.h>

namespace nx::vms::json_rpc {

using Error = nx::vms::api::JsonRpcError::Code;

static std::variant<api::JsonRpcRequest, api::JsonRpcResponse> deserialize(const QJsonValue& data)
{
    nx::vms::api::JsonRpcRequest jsonRpcRequest;
    try
    {
        QnJsonContext ctx;
        ctx.setStrictMode(true);
        jsonRpcRequest = QJson::deserializeOrThrow<api::JsonRpcRequest>(&ctx, data);
        return jsonRpcRequest;
    }
    catch (const QJson::InvalidParameterException& e)
    {
        return api::JsonRpcResponse::makeError(
            jsonRpcRequest.responseId(), {Error::invalidRequest, e.message().toStdString()});
    }
}

static QString idWithMethod(const api::JsonRpcRequest& request)
{
    return NX_FMT("request %1 with method %2", QJson::serialized(request.id), request.method);
}

void IncomingProcessor::processRequest(
    const QJsonValue& data, nx::utils::MoveOnlyFunc<void(QJsonValue)> handler)
{
    if (data.isArray())
    {
        auto list = data.toArray();
        if (list.empty())
        {
            NX_DEBUG(this, "Empty batch request received");
            return sendResponse(
                api::JsonRpcResponse::makeError(std::nullptr_t(),
                    {Error::invalidRequest, "Empty batch request"}),
                handler);
        }

        return processBatchRequest(list, std::move(handler));
    }

    auto requestOrError = deserialize(data);
    if (std::holds_alternative<api::JsonRpcResponse>(requestOrError))
        return sendResponse(std::get<api::JsonRpcResponse>(std::move(requestOrError)), handler);

    auto jsonRpcRequest = std::get<api::JsonRpcRequest>(std::move(requestOrError));
    if (!m_handler)
    {
        NX_DEBUG(this, "Ignore %1", idWithMethod(jsonRpcRequest));
        return sendResponse(
            api::JsonRpcResponse::makeError(jsonRpcRequest.responseId(),
                {Error::methodNotFound, "Method handler is not found"}),
            handler);
    }

    auto request = std::make_unique<Request>(std::move(jsonRpcRequest));
    auto requestPtr = request.get();
    m_requests.emplace(requestPtr, std::move(request));
    NX_DEBUG(this, "Start %1 %2", requestPtr, idWithMethod(requestPtr->jsonRpcRequest));
    m_handler(std::move(requestPtr->jsonRpcRequest),
        [this, requestPtr, handler = std::move(handler)](auto response)
        {
            NX_ASSERT(m_requests.erase(requestPtr), "Failed to find request %1", requestPtr);
            sendResponse(std::move(response), handler);
        });
}

void IncomingProcessor::processBatchRequest(
    const QJsonArray& list, nx::utils::MoveOnlyFunc<void(QJsonValue)> handler)
{
    std::vector<nx::vms::api::JsonRpcResponse> responses;
    std::unordered_map<Request*, std::unique_ptr<Request>> requests;
    std::vector<Request*> requestPtrs;
    for (const auto& item: list)
    {
        auto requestOrError = deserialize(item);
        if (std::holds_alternative<api::JsonRpcResponse>(requestOrError))
        {
            responses.emplace_back(std::get<api::JsonRpcResponse>(std::move(requestOrError)));
            continue;
        }

        auto jsonRpcRequest = std::get<api::JsonRpcRequest>(std::move(requestOrError));
        if (m_handler)
        {
            auto request = std::make_unique<Request>(std::move(jsonRpcRequest));
            auto requestPtr = request.get();
            requests.emplace(requestPtr, std::move(request));
            requestPtrs.push_back(requestPtr);
        }
        else
        {
            NX_DEBUG(this, "Ignore %1", idWithMethod(jsonRpcRequest));
            responses.emplace_back(api::JsonRpcResponse::makeError(jsonRpcRequest.responseId(),
                {Error::methodNotFound, "Method handler is not found"}));
        }
    }

    if (requests.empty())
    {
        QJsonValue serialized;
        QJson::serialize(responses, &serialized);
        NX_DEBUG(this, "No valid request in batch, responses %1", QJson::serialized(serialized));
        handler(std::move(serialized));
        return;
    }

    auto batchRequest = std::make_unique<BatchRequest>();
    batchRequest->requests = std::move(requests);
    batchRequest->responses = std::move(responses);
    batchRequest->handler = std::move(handler);
    auto batchRequestPtr = batchRequest.get();
    m_batchRequests.emplace(batchRequestPtr, std::move(batchRequest));
    for (const auto requestPtr: requestPtrs)
    {
        NX_DEBUG(this, "Start %1 in %2", requestPtr, batchRequestPtr);
        m_handler(std::move(requestPtr->jsonRpcRequest),
            [this, batchRequestPtr, requestPtr](auto response)
            {
                onBatchResponse(batchRequestPtr, requestPtr, std::move(response));
            });
    }
}

void IncomingProcessor::onBatchResponse(
    BatchRequest* batchRequest, Request* request, api::JsonRpcResponse response)
{
    NX_DEBUG(this, "Received response %1 for %2 in %3",
        response.result
            ? "id " + QJson::serialized(response.id)
            : "error " + QJson::serialized(response),
        request, batchRequest);

    auto it = m_batchRequests.find(batchRequest);
    if (!NX_ASSERT(it != m_batchRequests.end(), "Failed to find %1 for %2", batchRequest, request))
        return;

    if (!std::holds_alternative<std::nullptr_t>(response.id) || response.error)
        it->second->responses.push_back(std::move(response));

    NX_ASSERT(
        it->second->requests.erase(request), "Failed to find %1 in %2", request, batchRequest);
    if (it->second->requests.empty())
    {
        auto responses = std::move(it->second->responses);
        auto handler = std::move(it->second->handler);
        m_batchRequests.erase(it);

        NX_DEBUG(this, "End of %1", batchRequest);
        QJsonValue serialized;
        QJson::serialize(responses, &serialized);
        handler(std::move(serialized));
    }
}

void IncomingProcessor::sendResponse(nx::vms::api::JsonRpcResponse jsonRpcResponse,
    const nx::utils::MoveOnlyFunc<void(QJsonValue)>& handler)
{
    NX_DEBUG(this, jsonRpcResponse.error
        ? "Send error response " + QJson::serialized(jsonRpcResponse)
        : "Send response " + QJson::serialized(jsonRpcResponse.id));
    QJsonValue serialized;
    if (!std::holds_alternative<std::nullptr_t>(jsonRpcResponse.id) || jsonRpcResponse.error)
        QJson::serialize(jsonRpcResponse, &serialized);
    else
        NX_DEBUG(this, "Ignored response %1", QJson::serialized(jsonRpcResponse));
    handler(serialized);
}

} // namespace nx::vms::json_rpc
