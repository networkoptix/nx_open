// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "outgoing_processor.h"

#include <nx/reflect/json.h>

namespace std {

static QString toString(const nx::json_rpc::detail::OutgoingProcessor::Id& value)
{
    return std::holds_alternative<int>(value)
        ? nx::toString(std::get<int>(value))
        : '"' + std::get<QString>(value) + '"';
}

} // std

namespace nx::json_rpc::detail {

static OutgoingProcessor::Id toId(const ResponseId& id)
{
    if (std::holds_alternative<int>(id))
        return std::get<int>(id);
    return std::get<QString>(id);
}

void OutgoingProcessor::clear(SystemError::ErrorCode error)
{
    auto response =
        [error](const auto& id)
        {
            return Response::makeError(
                std::holds_alternative<int>(id)
                    ? ResponseId{std::get<int>(id)}
                    : ResponseId{std::get<QString>(std::move(id))},
                Error::transportError,
                    NX_FMT("Connection closed with error %1: %2", error, SystemError::toString(error))
                        .toStdString());
        };
    for (const auto& [id, handler]: m_awaitingResponses)
    {
        NX_DEBUG(this, "Terminating response with %1 id", id);
        if (!handler)
            continue;

        handler(response(id));
    }
    m_awaitingResponses.clear();
    for (auto& [key, batchResponse]: m_awaitingBatchResponseHolder)
    {
        NX_DEBUG(this, "Terminating batch response %1 with %2 ids", key, batchResponse.ids);
        if (!batchResponse.handler)
            continue;

        std::vector<Response> responses;
        for (auto& id: batchResponse.ids)
            responses.push_back(response(id));
        responses.insert(responses.end(),
            std::make_move_iterator(batchResponse.errors.begin()),
            std::make_move_iterator(batchResponse.errors.end()));
        batchResponse.handler(std::move(responses));
    }
    m_awaitingBatchResponseHolder.clear();
}

void OutgoingProcessor::processRequest(
    Request request, ResponseHandler handler, std::string serializedRequest)
{
    if (!request.id)
    {
        NX_DEBUG(this, "Send request with notification method %1", request.method);
        send(std::move(request), std::move(serializedRequest));
        if (handler)
            handler({});
        return;
    }

    auto id = *request.id;
    if (m_awaitingBatchResponses.contains(id) || m_awaitingResponses.contains(id))
    {
        NX_DEBUG(this, "Id %1 is already used", id);
        if (handler)
        {
            handler(Response::makeError(request.responseId(),
                Error::invalidRequest, "Invalid parameter 'id': Already used"));
        }
        return;
    }

    NX_DEBUG(this, "Send request with %1 id", id);
    m_awaitingResponses[std::move(id)] = std::move(handler);
    send(std::move(request), std::move(serializedRequest));
}

void OutgoingProcessor::processBatchRequest(
    std::vector<Request> jsonRpcRequests, BatchResponseHandler handler)
{
    std::vector<Request> goodRequests;
    AwaitingBatchResponse awaitingResponse;
    std::unordered_set<Id> ids;
    for (auto& jsonRpcRequest: jsonRpcRequests)
    {
        if (!jsonRpcRequest.id)
        {
            goodRequests.push_back(std::move(jsonRpcRequest));
            continue;
        }

        auto id = *jsonRpcRequest.id;
        if (m_awaitingBatchResponses.contains(id) || m_awaitingResponses.contains(id))
        {
            NX_DEBUG(this, "Id %1 is already used", id);
            awaitingResponse.errors.emplace_back(
                Response::makeError(jsonRpcRequest.responseId(),
                    Error::invalidRequest, "Invalid parameter 'id': Already used"));
            continue;
        }

        if (ids.contains(id))
        {
            NX_DEBUG(this, "Id %1 is already used in this batch request", id);
            awaitingResponse.errors.emplace_back(
                Response::makeError(jsonRpcRequest.responseId(),
                    Error::invalidRequest,
                        "Invalid parameter 'id': Already used in this batch request"));
            continue;
        }

        ids.insert(id);
        goodRequests.push_back(std::move(jsonRpcRequest));
        awaitingResponse.ids.push_back(std::move(id));
    }

    if (!NX_ASSERT(!goodRequests.empty()))
    {
        NX_DEBUG(this, "Batch request is completely invalid");
        if (handler)
            handler(std::move(awaitingResponse.errors));
        return;
    }

    if (ids.empty())
    {
        NX_DEBUG(this,
            "Send batch request with %1 notification methods",
            [&goodRequests]()
            {
                std::vector<std::string_view> result;
                for (const auto& r: goodRequests)
                    result.push_back(r.method);
                return result;
            }());
        send(std::move(goodRequests));
        if (handler)
            handler(std::move(awaitingResponse.errors));
        return;
    }

    ++m_nextBatchResponseKey;
    NX_DEBUG(this,
        "Send batch request %1 with %2 ids and %3 notification methods",
        m_nextBatchResponseKey, awaitingResponse.ids,
        [&goodRequests]()
        {
            std::vector<std::string_view> result;
            for (const auto& r: goodRequests)
            {
                if (!r.id)
                    result.push_back(r.method);
            }
            return result;
        }());
    for (const auto& id: awaitingResponse.ids)
        m_awaitingBatchResponses[id] = m_nextBatchResponseKey;
    awaitingResponse.handler = std::move(handler);
    m_awaitingBatchResponseHolder[m_nextBatchResponseKey] = std::move(awaitingResponse);
    send(std::move(goodRequests));
}

void OutgoingProcessor::send(Request request, std::string serializedRequest) const
{
    NX_ASSERT_HEAVY_CONDITION(
        serializedRequest.empty() || serializedRequest == nx::reflect::json::serialize(request),
        "Request: `%1`, serialized: `%2`", nx::reflect::json::serialize(request), serializedRequest);
    if (serializedRequest.empty())
        serializedRequest = nx::reflect::json::serialize(request);
    m_sendFunc(std::move(serializedRequest));
}

void OutgoingProcessor::send(std::vector<Request> requests) const
{
    m_sendFunc(nx::reflect::json::serialize(requests));
}

void OutgoingProcessor::onResponse(rapidjson::Document data)
{
    if (data.IsArray())
    {
        onArrayResponse(std::move(data));
        return;
    }

    Response response{.document = std::make_shared<rapidjson::Document>(std::move(data))};
    if (!response.deserialize(*response.document))
    {
        NX_DEBUG(this,
            "Failed to deserialize response: %1",
            nx::reflect::json_detail::getStringRepresentation(*response.document));
        response.error =
            Error{Error::parseError, "Failed to deserialize response", &*response.document};
    }

    if (std::holds_alternative<std::nullptr_t>(response.id))
    {
        if (m_awaitingResponses.size() == 1)
        {
            NX_DEBUG(this,
                "Apply %1 response to any previously sent request",
                nx::reflect::json_detail::getStringRepresentation(*response.document));
            auto handler = std::move(m_awaitingResponses.begin()->second);
            m_awaitingResponses.clear();
            if (handler)
                handler(std::move(response));
        }
        else
        {
            NX_DEBUG(this,
                "Ignore null or invalid response: %1",
                nx::reflect::json_detail::getStringRepresentation(*response.document));
        }
        return;
    }

    const auto id = toId(response.id);
    if (auto it = m_awaitingResponses.find(id); it != m_awaitingResponses.end())
    {
        NX_DEBUG(this, "Received response with %1 id", id);
        auto handler = std::move(it->second);
        m_awaitingResponses.erase(it);
        if (handler)
            handler(std::move(response));
        return;
    }

    NX_DEBUG(this,
        "Ignore response without request: %1",
        nx::reflect::json_detail::getStringRepresentation(*response.document));
}

void OutgoingProcessor::onArrayResponse(rapidjson::Document list)
{
    std::set<Key> batchResponseKeys;
    std::unordered_map<Id, Response> idResponses;
    std::vector<Response> nullResponses;
    auto holder{std::make_shared<rapidjson::Document>(std::move(list))};
    for (int i = 0; i < (int) holder->Size(); ++i)
    {
        Response response{.document = holder};
        if (!response.deserialize((*holder)[i]))
        {
            NX_DEBUG(this,
                "Failed to deserialize response item %1: %2", i,
                nx::reflect::json_detail::getStringRepresentation(*holder));
            response.error = Error{Error::parseError,
                NX_FMT("Failed to deserialize response item %1", i).toStdString(),
                &*holder};
            nullResponses.push_back(std::move(response));
            continue;
        }

        if (std::holds_alternative<std::nullptr_t>(response.id))
        {
            nullResponses.push_back(std::move(response));
            continue;
        }

        auto id = toId(response.id);
        auto it = m_awaitingBatchResponses.find(id);
        if (it == m_awaitingBatchResponses.end())
        {
            NX_DEBUG(this,
                "Ignore not-requested batch response item %1: %2", i,
                nx::reflect::json_detail::getStringRepresentation(*holder));
            continue;
        }

        idResponses.insert({std::move(id), std::move(response)});
        batchResponseKeys.insert(it->second);
        m_awaitingBatchResponses.erase(it);
    }

    if (idResponses.empty() && m_awaitingBatchResponseHolder.size() == 1)
    {
        auto& response = m_awaitingBatchResponseHolder.begin()->second;
        auto handler = std::move(response.handler);
        if (handler)
        {
            nullResponses.insert(nullResponses.end(),
                std::make_move_iterator(response.errors.begin()),
                std::make_move_iterator(response.errors.end()));
        }
        m_awaitingBatchResponseHolder.clear();
        m_awaitingBatchResponses.clear();
        if (handler)
            handler(std::move(nullResponses));
        return;
    }

    if (batchResponseKeys.empty())
    {
        NX_DEBUG(this,
            "Ignore null or invalid batch response: %1",
            nx::reflect::json_detail::getStringRepresentation(*holder));
        return;
    }

    if (batchResponseKeys.size() != 1)
    {
        NX_DEBUG(this,
            "Mixed ids of %1 batch requests in response: %1", batchResponseKeys,
            nx::reflect::json_detail::getStringRepresentation(*holder));
    }

    for (Key batchResponseKey: batchResponseKeys)
    {
        auto it = m_awaitingBatchResponseHolder.find(batchResponseKey);
        if (it == m_awaitingBatchResponseHolder.end())
        {
            NX_DEBUG(this, "Ignore %1 batch response with not found request", batchResponseKey);
            continue;
        }

        NX_DEBUG(this, "Received batch response %1 with %2 ids", batchResponseKey, it->second.ids);

        auto handler = std::move(it->second.handler);
        std::vector<Response> jsonRpcResponses;
        if (handler)
        {
            for (const auto& id: it->second.ids)
            {
                if (auto it = idResponses.find(id); it != idResponses.end())
                    jsonRpcResponses.push_back(std::move(it->second));
            }
            jsonRpcResponses.insert(jsonRpcResponses.end(),
                std::make_move_iterator(it->second.errors.begin()),
                std::make_move_iterator(it->second.errors.end()));
            jsonRpcResponses.insert(jsonRpcResponses.end(),
                std::make_move_iterator(nullResponses.begin()),
                std::make_move_iterator(nullResponses.end()));
        }
        m_awaitingBatchResponseHolder.erase(it);
        if (handler)
            handler(std::move(jsonRpcResponses));
    }
}

} // namespace nx::json_rpc::detail
