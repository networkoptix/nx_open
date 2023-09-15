// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_connection.h"

#include <nx/fusion/serialization/json_functions.h>
#include <nx/network/websocket/websocket.h>

#include "incoming_processor.h"
#include "detail/outgoing_processor.h"

namespace nx::vms::json_rpc {

using namespace detail;

static bool isResponse(const QJsonObject& object)
{
    return object.contains("result") || object.contains("error");
}

static bool isResponse(const QJsonArray& list)
{
    bool result = !list.empty() && isResponse(list.begin()->toObject());
    for (int i = 1; i < list.size(); ++i)
    {
        if (isResponse(list[i].toObject()) != result)
        {
            throw api::JsonRpcError{
                api::JsonRpcError::InvalidJson, "Mixed request and response in a batch"};
        }
    }
    return result;
}

static bool isResponse(const QJsonValue& value)
{
    if (value.isArray())
        return isResponse(value.toArray());

    if (value.isObject())
        return isResponse(value.toObject());

    return false;
}

WebSocketConnection::WebSocketConnection(
    std::unique_ptr<nx::network::websocket::WebSocket> socket,
    OnDone onDone,
    RequestHandler requestHandler)
    :
    m_onDone(std::move(onDone)),
    m_socket(std::move(socket))
{
    if (requestHandler)
    {
        m_incomingProcessor = std::make_unique<IncomingProcessor>(
            [this, requestHandler = std::move(requestHandler)](auto request, auto responseHandler)
            {
                requestHandler(
                    std::move(request),
                    [this, handler = std::move(responseHandler)](auto response) mutable
                    {
                        post(
                            [response = std::move(response), handler = std::move(handler)]() mutable
                            {
                                handler(std::move(response));
                            });
                    },
                    this);
            });
    }
    else
    {
        m_incomingProcessor = std::make_unique<IncomingProcessor>();
    }
    m_outgoingProcessor =
        std::make_unique<OutgoingProcessor>([this](auto value) { send(std::move(value)); });
    base_type::bindToAioThread(m_socket->getAioThread());
    m_socket->start();
    readNextMessage();
}

WebSocketConnection::~WebSocketConnection()
{
    pleaseStopSync();
}

void WebSocketConnection::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_socket->bindToAioThread(aioThread);
}

void WebSocketConnection::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_socket->pleaseStopSync();
    std::queue<QJsonValue> empty;
    m_queuedRequests.swap(empty);
}

void WebSocketConnection::send(
    nx::vms::api::JsonRpcRequest jsonRpcRequest, ResponseHandler handler)
{
    executeInAioThreadSync(
        [this, jsonRpcRequest = std::move(jsonRpcRequest), handler = std::move(handler)]() mutable
        {
            m_outgoingProcessor->processRequest(std::move(jsonRpcRequest), std::move(handler));
        });
}

void WebSocketConnection::send(
    std::vector<nx::vms::api::JsonRpcRequest> jsonRpcRequests, BatchResponseHandler handler)
{
    executeInAioThreadSync(
        [this, jsonRpcRequests = std::move(jsonRpcRequests), handler = std::move(handler)]() mutable
        {
            m_outgoingProcessor->processBatchRequest(std::move(jsonRpcRequests), std::move(handler));
        });
}

void WebSocketConnection::readNextMessage()
{
    auto buffer = std::make_unique<nx::Buffer>();
    auto bufferPtr = buffer.get();
    m_socket->readSomeAsync(
        bufferPtr,
        [this, buffer = std::move(buffer)](auto errorCode, auto /*bytesTransferred*/)
        {
            if (errorCode != SystemError::noError)
            {
                NX_DEBUG(this, "Failed to read next message with error code %1", errorCode);
                m_outgoingProcessor->clear(errorCode);
                m_onDone(this);
                return;
            }

            readHandler(*buffer.get());
            readNextMessage();
        });
}

void WebSocketConnection::readHandler(const nx::Buffer& buffer)
{
    try
    {
        QJsonValue data;
        QString error;
        if (!QJsonDetail::deserialize_json(buffer.toRawByteArray(), &data, &error))
            throw api::JsonRpcError{api::JsonRpcError::InvalidJson, error.toStdString()};

        if (isResponse(data))
            m_outgoingProcessor->onResponse(data);
        else
            processRequest(data);
    }
    catch (api::JsonRpcError e)
    {
        NX_DEBUG(this, "Error %1 processing received message", QJson::serialized(e));
        QJsonValue serialized;
        nx::vms::api::JsonRpcResponse jsonRpcResponse;
        jsonRpcResponse.error = std::move(e);
        QJson::serialize(jsonRpcResponse, &serialized);
        send(std::move(serialized));
    }
}

void WebSocketConnection::processRequest(const QJsonValue& data)
{
    m_queuedRequests.push(data);
    if (m_queuedRequests.size() == 1)
        processQueuedRequest();
}

void WebSocketConnection::processQueuedRequest()
{
    m_incomingProcessor->processRequest(m_queuedRequests.front(),
        [this](QJsonValue response)
        {
            if (!response.isNull())
                send(std::move(response));
            m_queuedRequests.pop();
            if (!m_queuedRequests.empty())
                processQueuedRequest();
        });
}

void WebSocketConnection::send(QJsonValue data)
{
    auto buffer = std::make_unique<nx::Buffer>(QJson::serialized(data));
    auto bufferPtr = buffer.get();
    m_socket->sendAsync(bufferPtr,
        [buffer = std::move(buffer), this](auto errorCode, auto /*bytesTransferred*/)
        {
            if (errorCode != SystemError::noError)
            {
                NX_DEBUG(this, "Failed to send message with error code %1", errorCode);
                m_outgoingProcessor->clear(errorCode);
                m_onDone(this);
            }
        });
}

} // namespace nx::vms::json_rpc
