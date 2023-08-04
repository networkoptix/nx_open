// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "outgoing_processor.h"

#include <nx/fusion/serialization/json_functions.h>

namespace std {

static QString toString(const nx::vms::json_rpc::detail::OutgoingProcessor::Id& value)
{
    return std::holds_alternative<int>(value)
        ? nx::toString(std::get<int>(value))
        : '"' + std::get<QString>(value) + '"';
}

} // std

namespace nx::vms::json_rpc::detail {

static OutgoingProcessor::Id toId(const api::JsonRpcResponseId& id)
{
    if (std::holds_alternative<int>(id))
        return std::get<int>(id);
    return std::get<QString>(id);
}

using Error = nx::vms::api::JsonRpcError::Code;

void OutgoingProcessor::clear(SystemError::ErrorCode error)
{
    auto response = api::JsonRpcResponse::makeError(std::nullptr_t(),
        {Error::InternalError,
            NX_FMT("Connection closed with error %1: %2", error, SystemError::toString(error))
                .toStdString()});
    for (const auto& [id, handler]: m_awaitingResponses)
    {
        NX_DEBUG(this, "Terminating response with %1 id", id);
        if (!handler)
            continue;

        if (std::holds_alternative<int>(id))
            response.id = std::get<int>(id);
        else
            response.id = std::get<QString>(id);
        handler(response);
    }
    m_awaitingResponses.clear();
    for (auto& [key, batchResponse]: m_awaitingBatchResponseHolder)
    {
        NX_DEBUG(this, "Terminating batch response %1 with %2 ids", key, batchResponse.ids);
        if (!batchResponse.handler)
            continue;

        std::vector<api::JsonRpcResponse> responses;
        for (auto& id: batchResponse.ids)
        {
            if (std::holds_alternative<int>(id))
                response.id = std::get<int>(id);
            else
                response.id = std::get<QString>(std::move(id));
            responses.push_back(response);
        }
        responses.insert(responses.end(),
            std::make_move_iterator(batchResponse.errors.begin()),
            std::make_move_iterator(batchResponse.errors.end()));
        batchResponse.handler(std::move(responses));
    }
    m_awaitingBatchResponseHolder.clear();
}

void OutgoingProcessor::processRequest(
    nx::vms::api::JsonRpcRequest jsonRpcRequest, ResponseHandler handler)
{
    if (!jsonRpcRequest.id)
    {
        NX_DEBUG(this, "Send request with notification method %1", jsonRpcRequest.method);
        send(std::move(jsonRpcRequest));
        if (handler)
            handler({});
        return;
    }

    auto id = *jsonRpcRequest.id;
    if (m_awaitingBatchResponses.contains(id) || m_awaitingResponses.contains(id))
    {
        NX_DEBUG(this, "Id %1 is already used", id);
        if (handler)
        {
            handler(api::JsonRpcResponse::makeError(jsonRpcRequest.responseId(),
                {Error::InvalidRequest, "Invalid parameter 'id': Already used"}));
        }
        return;
    }

    NX_DEBUG(this, "Send request with %1 id", id);
    m_awaitingResponses[std::move(id)] = std::move(handler);
    send(std::move(jsonRpcRequest));
}

void OutgoingProcessor::processBatchRequest(
    std::vector<nx::vms::api::JsonRpcRequest> jsonRpcRequests, BatchResponseHandler handler)
{
    std::vector<nx::vms::api::JsonRpcRequest> goodRequests;
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
                api::JsonRpcResponse::makeError(jsonRpcRequest.responseId(),
                    {Error::InvalidRequest, "Invalid parameter 'id': Already used"}));
            continue;
        }

        if (ids.contains(id))
        {
            NX_DEBUG(this, "Id %1 is already used in this batch request", id);
            awaitingResponse.errors.emplace_back(
                api::JsonRpcResponse::makeError(jsonRpcRequest.responseId(),
                    {Error::InvalidRequest,
                        "Invalid parameter 'id': Already used in this batch request"}));
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

void OutgoingProcessor::send(nx::vms::api::JsonRpcRequest jsonRpcRequest) const
{
    QJsonValue serialized;
    QJson::serialize(jsonRpcRequest, &serialized);
    m_sendFunc(std::move(serialized));
}

void OutgoingProcessor::send(
    std::vector<nx::vms::api::JsonRpcRequest> jsonRpcRequests) const
{
    QJsonValue serialized;
    QJson::serialize(jsonRpcRequests, &serialized);
    m_sendFunc(std::move(serialized));
}

void OutgoingProcessor::onResponse(const QJsonValue& data)
{
    if (data.isArray())
    {
        onResponse(data.toArray());
        return;
    }

    nx::vms::api::JsonRpcResponse jsonRpcResponse;
    if (!QJson::deserialize(data, &jsonRpcResponse))
    {
        NX_DEBUG(this, "Failed to deserialize response: %1", data);
        jsonRpcResponse.error = nx::vms::api::JsonRpcError{
            Error::InvalidJson, "Failed to deserialize response", data};
    }

    if (std::holds_alternative<std::nullptr_t>(jsonRpcResponse.id))
    {
        if (m_awaitingResponses.size() == 1)
        {
            NX_DEBUG(this, "Apply %1 response to any previously sent request", data);
            auto handler = std::move(m_awaitingResponses.begin()->second);
            m_awaitingResponses.clear();
            if (handler)
                handler(std::move(jsonRpcResponse));
        }
        else
        {
            NX_DEBUG(this, "Ignore null or invalid response: %1", data);
        }
        return;
    }

    const auto id = toId(jsonRpcResponse.id);
    if (auto it = m_awaitingResponses.find(id); it != m_awaitingResponses.end())
    {
        NX_DEBUG(this, "Received response with %1 id", id);
        auto handler = std::move(it->second);
        m_awaitingResponses.erase(it);
        if (handler)
            handler(std::move(jsonRpcResponse));
        return;
    }

    NX_DEBUG(this, "Ignore response without request: %1", data);
}

void OutgoingProcessor::onResponse(const QJsonArray& list)
{
    std::set<Key> batchResponseKeys;
    std::unordered_map<Id, nx::vms::api::JsonRpcResponse> idResponses;
    std::vector<nx::vms::api::JsonRpcResponse> nullResponses;
    for (int i = 0; i < list.size(); ++i)
    {
        nx::vms::api::JsonRpcResponse jsonRpcResponse;
        if (!QJson::deserialize(list[i], &jsonRpcResponse))
        {
            NX_DEBUG(this, "Failed to deserialize response item %1: %2", i, list);
            jsonRpcResponse.error = nx::vms::api::JsonRpcError{Error::InvalidJson,
                NX_FMT("Failed to deserialize response item %1", i).toStdString(),
                list};
            nullResponses.push_back(std::move(jsonRpcResponse));
            continue;
        }

        if (std::holds_alternative<std::nullptr_t>(jsonRpcResponse.id))
        {
            nullResponses.push_back(std::move(jsonRpcResponse));
            continue;
        }

        auto id = toId(jsonRpcResponse.id);
        auto it = m_awaitingBatchResponses.find(id);
        if (it == m_awaitingBatchResponses.end())
        {
            NX_DEBUG(this, "Ignore not-requested batch response item %1: %2", i, list);
            continue;
        }

        idResponses.insert({std::move(id), std::move(jsonRpcResponse)});
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
        NX_DEBUG(this, "Ignore null or invalid batch response: %1", list);
        return;
    }

    if (batchResponseKeys.size() != 1)
        NX_DEBUG(this, "Mixed ids of %1 batch requests in response: %1", batchResponseKeys, list);

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
        std::vector<nx::vms::api::JsonRpcResponse> jsonRpcResponses;
        if (handler)
        {
            for (const auto& id: it->second.ids)
            {
                if (auto it = idResponses.find(id); it != idResponses.end())
                    jsonRpcResponses.push_back(it->second);
            }
            jsonRpcResponses.insert(jsonRpcResponses.end(),
                std::make_move_iterator(it->second.errors.begin()),
                std::make_move_iterator(it->second.errors.end()));
            jsonRpcResponses.insert(
                jsonRpcResponses.end(), nullResponses.begin(), nullResponses.end());
        }
        m_awaitingBatchResponseHolder.erase(it);
        if (handler)
            handler(std::move(jsonRpcResponses));
    }
}

} // namespace nx::vms::json_rpc::detail
