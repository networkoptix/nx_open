// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "incoming_processor.h"

#include <nx/reflect/json.h>

namespace nx::json_rpc {

static std::variant<Request, Response> deserialize(
    rapidjson::Document::AllocatorType allocator, rapidjson::Value& value)
{
    Request request{std::move(allocator)};
    auto r{nx::reflect::json::deserialize({value}, &request)};
    if (r)
        return std::variant<Request, Response>{std::move(request)};

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
    rapidjson::Document data, nx::MoveOnlyFunc<void(std::string)> handler)
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

    auto requestOrError = deserialize(data.GetAllocator(), data);
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

    static std::atomic<uint64_t> request;
    auto index = request.fetch_add(1);
    NX_DEBUG(this, "Start #%1 %2", index, idWithMethod(jsonRpcRequest));
    m_handler(std::move(jsonRpcRequest),
        [this, index, handler = std::move(handler)](auto response)
        {
            NX_DEBUG(this, "End #%1", index);
            sendResponse(std::move(response), handler);
        });
}

void IncomingProcessor::processBatchRequest(
    rapidjson::Document list, nx::MoveOnlyFunc<void(std::string)> handler)
{
    std::vector<Response> responses;
    std::vector<Request> requests;
    for (auto it = list.Begin(); it != list.End(); ++it)
    {
        auto& item = *it;
        auto requestOrError = deserialize(list.GetAllocator(), item);
        if (std::holds_alternative<Response>(requestOrError))
        {
            responses.emplace_back(std::get<Response>(std::move(requestOrError)));
            continue;
        }

        auto jsonRpcRequest = std::get<Request>(std::move(requestOrError));
        if (m_handler)
        {
            requests.push_back(std::move(jsonRpcRequest));
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

    auto batchRequest = std::make_unique<BatchRequest>(
        BatchRequest{.requests = std::vector<bool>(requests.size(), true)});
    batchRequest->responses = std::move(responses);
    batchRequest->handler = std::move(handler);
    auto batchRequestPtr = batchRequest.get();
    m_batchRequests.emplace(batchRequestPtr, std::move(batchRequest));
    int index = 0;
    for (auto& request: requests)
    {
        NX_DEBUG(this, "Start %1 request in %2", index, batchRequestPtr);
        m_handler(std::move(request),
            [this, batchRequestPtr, index](auto response)
            {
                onBatchResponse(batchRequestPtr, index, std::move(response));
            });
        ++index;
    }
}

void IncomingProcessor::onBatchResponse(BatchRequest* batchRequest, int request, Response response)
{
    NX_DEBUG(this, "Received response %1 for %2 request in %3",
        response.result
            ? "id " + nx::reflect::json::serialize(response.id)
            : "error " + nx::reflect::json::serialize(response),
        request, batchRequest);

    auto it = m_batchRequests.find(batchRequest);
    if (!NX_ASSERT(it != m_batchRequests.end(), "Failed to find %1 for %2", batchRequest, request))
        return;

    if (!std::holds_alternative<std::nullptr_t>(response.id)
        || (response.error && response.error->code == Error::invalidRequest))
    {
        it->second->responses.push_back(std::move(response));
    }

    if (NX_ASSERT(it->second->requests[request],
        "Response for %1 in %2 is already processed", request, batchRequest))
    {
        it->second->requests[request] = false;
    }
    if (it->second->requests == std::vector<bool>(it->second->requests.size()))
    {
        auto responses = std::move(it->second->responses);
        auto handler = std::move(it->second->handler);
        m_batchRequests.erase(it);

        NX_DEBUG(this, "End of %1", batchRequest);
        handler(responses.empty() ? std::string{} : nx::reflect::json::serialize(responses));
    }
}

void IncomingProcessor::sendResponse(
    Response response, const nx::MoveOnlyFunc<void(std::string)>& handler)
{
    std::string value;
    if (!std::holds_alternative<std::nullptr_t>(response.id)
        || (response.error && response.error->code == Error::invalidRequest))
    {
        value = nx::reflect::json::serialize(response);
        NX_DEBUG(this, response.error
            ? "Send error response " + value
            : "Send response " + nx::reflect::json::serialize(response.id));
    }
    else
    {
        NX_DEBUG(this, "Ignored response " + nx::reflect::json::serialize(response));
    }
    handler(std::move(value));
}

} // namespace nx::json_rpc
