// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_connection.h"

#include <nx/fusion/serialization/json_functions.h>
#include <nx/network/websocket/websocket.h>

#include "incoming_processor.h"
#include "detail/outgoing_processor.h"

namespace nx::vms::json_rpc {

using namespace detail;

namespace {

void logMessage(
    const char* action, const nx::network::SocketAddress& address, const nx::Buffer& buffer)
{
    NX_DEBUG(nx::log::kHttpTag, "JSON-RPC %1 %2\n%3", action, address,
        nx::log::isToBeLogged(nx::log::Level::verbose, nx::log::kHttpTag)
            ? buffer
            : buffer.substr(0, 1024 * 5)); //< Should be enough for 5 devices.
}

bool isResponse(const QJsonObject& object)
{
    return object.contains("result") || object.contains("error");
}

bool isResponse(const QJsonArray& list)
{
    bool result = !list.empty() && isResponse(list.begin()->toObject());
    for (int i = 1; i < list.size(); ++i)
    {
        if (isResponse(list[i].toObject()) != result)
        {
            throw api::JsonRpcError{
                api::JsonRpcError::invalidRequest, "Mixed request and response in a batch"};
        }
    }
    return result;
}

bool isResponse(const QJsonValue& value)
{
    if (value.isArray())
        return isResponse(value.toArray());

    if (value.isObject())
        return isResponse(value.toObject());

    return false;
}

} // namespace

std::shared_ptr<WebSocketConnection> WebSocketConnection::create(
    std::unique_ptr<nx::network::websocket::WebSocket> socket,
    OnDone onDone,
    RequestHandler requestHandler)
{
    std::shared_ptr<WebSocketConnection> sharedConnection{
        new WebSocketConnection{std::move(socket), std::move(onDone)}};
    sharedConnection->m_self = sharedConnection;
    std::weak_ptr<WebSocketConnection> weakConnection{sharedConnection};
    auto connectionPointer{sharedConnection.get()};
    if (requestHandler)
    {
        sharedConnection->m_incomingProcessor = std::make_unique<IncomingProcessor>(
            [weakConnection, connectionPointer, requestHandler = std::move(requestHandler)](
                auto request, auto responseHandler)
            {
                requestHandler(std::move(request),
                    [weakConnection, handler = std::move(responseHandler)](auto response) mutable
                    {
                        if (auto lock = weakConnection.lock())
                        {
                            lock->dispatch(
                                [response = std::move(response), handler = std::move(handler)]() mutable
                                {
                                    handler(std::move(response));
                                });
                        }
                        else
                        {
                            handler(std::move(response));
                        }
                    },
                    connectionPointer);
            });
    }
    else
    {
        sharedConnection->m_incomingProcessor = std::make_unique<IncomingProcessor>();
    }
    sharedConnection->m_outgoingProcessor = std::make_unique<OutgoingProcessor>(
        [weakConnection](auto value)
        {
            if (auto lock = weakConnection.lock())
                lock->send(std::move(value));
        });
    sharedConnection->m_socket->start();
    return sharedConnection;
}

WebSocketConnection::WebSocketConnection(
    std::unique_ptr<nx::network::websocket::WebSocket> socket, OnDone onDone)
    :
    m_onDone(std::move(onDone)),
    m_socket(std::move(socket))
{
    base_type::bindToAioThread(m_socket->getAioThread());
    if (auto socket = m_socket->socket())
        m_address = socket->getForeignAddress();
}

void WebSocketConnection::start()
{
    readNextMessage();
}

WebSocketConnection::~WebSocketConnection()
{
    m_onDone(this);
}

void WebSocketConnection::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_socket->bindToAioThread(aioThread);
}

void WebSocketConnection::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_guards.clear();
    m_socket->pleaseStopSync();
    std::queue<QJsonValue> empty;
    m_queuedRequests.swap(empty);
}

void WebSocketConnection::send(
    nx::vms::api::JsonRpcRequest request, ResponseHandler handler, QByteArray serializedRequest)
{
    dispatch(
        [self = m_self,
            request = std::move(request),
            handler = std::move(handler),
            serializedRequest = std::move(serializedRequest)]() mutable
        {
            if (auto lock = self.lock())
            {
                lock->m_outgoingProcessor->processRequest(
                    std::move(request), std::move(handler), std::move(serializedRequest));
            }
        });
}

void WebSocketConnection::send(
    std::vector<nx::vms::api::JsonRpcRequest> jsonRpcRequests, BatchResponseHandler handler)
{
    dispatch(
        [self = m_self,
            jsonRpcRequests = std::move(jsonRpcRequests),
            handler = std::move(handler)]() mutable
        {
            if (auto lock = self.lock())
            {
                lock->m_outgoingProcessor->processBatchRequest(
                    std::move(jsonRpcRequests), std::move(handler));
            }
        });
}

void WebSocketConnection::readNextMessage()
{
    auto buffer = std::make_unique<nx::Buffer>();
    auto bufferPtr = buffer.get();
    m_socket->readSomeAsync(bufferPtr,
        [self = m_self, buffer = std::move(buffer)](auto errorCode, auto /*bytesTransferred*/)
        {
            if (errorCode != SystemError::noError)
            {
                if (auto lock = self.lock())
                {
                    NX_DEBUG(
                        lock.get(), "Failed to read next message with error code %1", errorCode);
                    lock->m_outgoingProcessor->clear(errorCode);
                    lock->m_onDone(lock.get());
                }
                return;
            }

            if (auto lock = self.lock())
            {
                lock->readHandler(*buffer.get());
                lock->readNextMessage();
            }
        });
}

void WebSocketConnection::readHandler(const nx::Buffer& buffer)
{
    logMessage("receive from", m_address, buffer);
    try
    {
        QJsonValue data;
        QString error;
        if (!QJsonDetail::deserialize_json(buffer.toRawByteArray(), &data, &error))
            throw api::JsonRpcError{api::JsonRpcError::parseError, error.toStdString()};

        if (isResponse(data))
            m_outgoingProcessor->onResponse(data);
        else
            processRequest(data);
    }
    catch (api::JsonRpcError e)
    {
        NX_DEBUG(this, "Error %1 processing received message", QJson::serialized(e));
        send(QJson::serialized(api::JsonRpcResponse::makeError(std::nullptr_t(), std::move(e))));
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
        [self = m_self](QJsonValue response)
        {
            if (auto lock = self.lock())
            {
                if (!response.isNull())
                    lock->send(QJson::serialized(response));
                lock->m_queuedRequests.pop();
                if (!lock->m_queuedRequests.empty())
                    lock->processQueuedRequest();
            }
        });
}

void WebSocketConnection::send(QByteArray data)
{
    auto buffer = std::make_unique<nx::Buffer>(std::move(data));
    auto bufferPtr = buffer.get();
    logMessage("send to", m_address, *bufferPtr);
    m_socket->sendAsync(bufferPtr,
        [self = m_self, buffer = std::move(buffer)](auto errorCode, auto /*bytesTransferred*/)
        {
            if (errorCode == SystemError::noError)
                return;

            if (auto lock = self.lock())
            {
                NX_DEBUG(lock.get(), "Failed to send message with error code %1", errorCode);
                lock->m_outgoingProcessor->clear(errorCode);
                lock->m_onDone(lock.get());
            }
        });
}

void WebSocketConnection::addGuard(const QString& id, nx::utils::Guard guard)
{
    if (!NX_ASSERT(guard))
        return;

    dispatch(
        [self = m_self, id, g = std::move(guard)]() mutable
        {
            if (auto lock = self.lock())
            {
                auto& guard = lock->m_guards[id];
                NX_VERBOSE(lock.get(), "%1 guard for %2", guard ? "Adding" : "Replacing", id);
                guard = std::move(g);
            }
        });
}

void WebSocketConnection::removeGuard(const QString& id)
{
    dispatch(
        [self = m_self, id]() mutable
        {
            if (auto lock = self.lock())
            {
                NX_VERBOSE(lock.get(), "Removing guard for %1", id);
                lock->m_guards.erase(id);
            }
        });
}

} // namespace nx::vms::json_rpc
