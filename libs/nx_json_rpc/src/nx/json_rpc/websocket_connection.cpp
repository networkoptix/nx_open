// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_connection.h"

#include <nx/network/websocket/websocket.h>

#include "detail/outgoing_processor.h"
#include "incoming_processor.h"

namespace nx::json_rpc {

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

bool isObjectResponse(const rapidjson::Value& object)
{
    return object.HasMember("result") || object.HasMember("error");
}

std::pair<bool, std::optional<Response>> isArrayResponse(const rapidjson::Value& list)
{
    bool result = !list.Empty() && list[0].IsObject() && isObjectResponse(list[0]);
    std::optional<Response> error;
    for (int i = 1; i < (int) list.Size(); ++i)
    {
        if (list.IsObject() && isObjectResponse(list[i]) != result)
        {
            error = Response::makeError(
                std::nullptr_t{}, Error::invalidRequest, "Mixed request and response in a batch");
            break;
        }
    }
    return {result, std::move(error)};
}

std::pair<bool, std::optional<Response>> isResponse(const rapidjson::Value& value)
{
    if (value.IsArray())
        return isArrayResponse(value);

    return {value.IsObject() && isObjectResponse(value), std::optional<Response>{}};
}

} // namespace

WebSocketConnection::WebSocketConnection(
    std::unique_ptr<nx::network::websocket::WebSocket> webSocket, OnDone onDone)
    :
    m_onDone(std::move(onDone)),
    m_socket(std::move(webSocket))
{
    base_type::bindToAioThread(m_socket->getAioThread());
    if (auto socket = m_socket->socket())
        m_address = socket->getForeignAddress();
}

void WebSocketConnection::start(
    std::weak_ptr<WebSocketConnection> self, RequestHandler requestHandler)
{
    m_self = std::move(self);
    m_outgoingProcessor = std::make_unique<OutgoingProcessor>(
        [self = m_self](auto value)
        {
            if (auto lock = self.lock())
                lock->send(std::move(value));
        });
    if (requestHandler)
    {
        m_incomingProcessor = std::make_unique<IncomingProcessor>(
            [requestHandler = std::move(requestHandler), thisPointer = this](
                const auto& request, auto responseHandler)
            {
                requestHandler(request,
                    [handler = std::move(responseHandler)](auto response) mutable
                    {
                        handler(std::move(response));
                    },
                    thisPointer);
            });
    }
    else
    {
        m_incomingProcessor = std::make_unique<IncomingProcessor>();
    }
    m_socket->start();
    readNextMessage();
}

WebSocketConnection::~WebSocketConnection()
{
    nx::utils::moveAndCallOptional(m_onDone, SystemError::noError, this);
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
    std::queue<rapidjson::Document> empty;
    m_queuedRequests.swap(empty);
}

void WebSocketConnection::send(
    Request request, ResponseHandler handler, std::string serializedRequest)
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
    std::vector<Request> jsonRpcRequests, BatchResponseHandler handler)
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
                    nx::utils::moveAndCallOptional(lock->m_onDone, errorCode, lock.get());
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
    rapidjson::Document data;
    data.Parse(buffer.data(), buffer.size());
    if (data.HasParseError())
    {
        auto r{nx::reflect::json::serialize(Response::makeError(std::nullptr_t{},
            Error::parseError, nx::reflect::json_detail::parseErrorToString(data)))};
        NX_DEBUG(this, "Error processing received message: " + r);
        send(std::move(r));
        return;
    }

    if (auto [isResponse_, errorResponse] = isResponse(data); errorResponse)
    {
        auto r{nx::reflect::json::serialize(*errorResponse)};
        NX_DEBUG(this, "Error processing received message: " + r);
        send(std::move(r));
    }
    else
    {
        if (isResponse_)
            m_outgoingProcessor->onResponse(std::move(data));
        else
            processRequest(std::move(data));
    }
}

void WebSocketConnection::processRequest(rapidjson::Document data)
{
    m_queuedRequests.push(std::move(data));
    if (m_queuedRequests.size() == 1)
        processQueuedRequest();
}

void WebSocketConnection::processQueuedRequest()
{
    m_incomingProcessor->processRequest(std::move(m_queuedRequests.front()),
        [self = m_self](std::string response)
        {
            if (auto lock = self.lock())
            {
                if (!response.empty())
                    lock->send(std::move(response));
                lock->m_queuedRequests.pop();
                if (!lock->m_queuedRequests.empty())
                    lock->processQueuedRequest();
            }
        });
}

void WebSocketConnection::send(std::string data)
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
                nx::utils::moveAndCallOptional(lock->m_onDone, errorCode, lock.get());
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

} // namespace nx::json_rpc
