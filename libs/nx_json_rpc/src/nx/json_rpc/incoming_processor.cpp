// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "incoming_processor.h"

#include <nx/reflect/json.h>

namespace nx::json_rpc {

static std::variant<Request, Response> deserialize(
    rapidjson::Value& value, std::shared_ptr<rapidjson::Document> document)
{
    Request request{std::move(document)};
    auto r{request.deserialize(value)};
    if (r)
        return request;

    return Response::makeError(request.responseId(), Error::invalidRequest,
        r.firstNonDeserializedField
            ? nx::json::InvalidParameterException{{
                QString::fromStdString(*r.firstNonDeserializedField),
                QString::fromStdString(r.firstBadFragment)}}.message().toStdString()
            : r.errorDescription);
}

static QString idWithMethod(const Request& request)
{
    return NX_FMT(
        "request %1 with method %2", nx::reflect::json::serialize(request.id), request.method);
}

void IncomingProcessor::processRequest(
    rapidjson::Document data, nx::utils::MoveOnlyFunc<void(std::string)> handler)
{
    if (data.IsArray())
    {
        if (data.Empty())
        {
            NX_DEBUG(this, "Empty batch request received");
            return sendResponse(
                Response::makeError(
                    std::nullptr_t{}, Error::invalidRequest, "Empty batch request"),
                handler);
        }

        return processBatchRequest(std::move(data), std::move(handler));
    }

    auto holder = std::make_shared<rapidjson::Document>(std::move(data));
    auto requestOrError = deserialize(*holder, holder);
    if (std::holds_alternative<Response>(requestOrError))
        return sendResponse(std::get<Response>(std::move(requestOrError)), handler);

    auto jsonRpcRequest = std::get<Request>(std::move(requestOrError));
    if (!m_handler)
    {
        NX_DEBUG(this, "Ignore %1", idWithMethod(jsonRpcRequest));
        return sendResponse(
            Response::makeError(
                jsonRpcRequest.responseId(), Error::methodNotFound, "Method handler is not found"),
            handler);
    }

    auto request = std::make_unique<Request>(std::move(jsonRpcRequest));
    auto requestPtr = request.get();
    m_requests.emplace(requestPtr, std::move(request));
    NX_DEBUG(this, "Start %1 %2", (void*) requestPtr, idWithMethod(*requestPtr));
    m_handler(*requestPtr,
        [this, requestPtr, handler = std::move(handler)](auto response)
        {
            NX_ASSERT(m_requests.erase(requestPtr), "Failed to find request %1", (void*) requestPtr);
            sendResponse(std::move(response), handler);
        });
}

void IncomingProcessor::processBatchRequest(
    rapidjson::Document list, nx::utils::MoveOnlyFunc<void(std::string)> handler)
{
    std::vector<Response> responses;
    std::unordered_map<Request*, std::unique_ptr<Request>> requests;
    std::vector<Request*> requestPtrs;
    auto holder = std::make_shared<rapidjson::Document>(std::move(list));
    for (auto it = holder->Begin(); it != holder->End(); ++it)
    {
        auto& item = *it;
        auto requestOrError = deserialize(item, holder);
        if (std::holds_alternative<Response>(requestOrError))
        {
            responses.emplace_back(std::get<Response>(std::move(requestOrError)));
            continue;
        }

        auto jsonRpcRequest = std::get<Request>(std::move(requestOrError));
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
            responses.emplace_back(Response::makeError(jsonRpcRequest.responseId(),
                Error::methodNotFound, "Method handler is not found"));
        }
    }

    if (requests.empty())
    {
        auto serialized{nx::reflect::json::serialize(responses)};
        NX_DEBUG(this, "No valid request in batch, responses %1", serialized);
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
        m_handler(*requestPtr,
            [this, batchRequestPtr, requestPtr](auto response)
            {
                onBatchResponse(batchRequestPtr, requestPtr, std::move(response));
            });
    }
}

void IncomingProcessor::onBatchResponse(
    BatchRequest* batchRequest, Request* request, Response response)
{
    NX_DEBUG(this, "Received response %1 for %2 in %3",
        response.result
            ? "id " + nx::reflect::json::serialize(response.id)
            : "error " + nx::reflect::json::serialize(response),
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
        handler(nx::reflect::json::serialize(responses));
    }
}

void IncomingProcessor::sendResponse(
    Response response, const nx::utils::MoveOnlyFunc<void(std::string)>& handler)
{
    NX_DEBUG(this, response.error
        ? "Send error response " + nx::reflect::json::serialize(response)
        : "Send response " + nx::reflect::json::serialize(response.id));
    std::string value;
    if (!std::holds_alternative<std::nullptr_t>(response.id) || response.error)
        value = nx::reflect::json::serialize(response);
    else
        NX_DEBUG(this, "Ignored response %1", nx::reflect::json::serialize(response));
    handler(std::move(value));
}

} // namespace nx::json_rpc
