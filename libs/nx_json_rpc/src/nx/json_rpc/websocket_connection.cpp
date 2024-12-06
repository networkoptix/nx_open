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
    std::unique_ptr<nx::network::websocket::WebSocket> webSocket,
    OnDone onDone)
    :
    m_onDone(std::move(onDone)),
    m_incomingProcessor{std::make_unique<IncomingProcessor>()},
    m_outgoingProcessor{
        std::make_unique<OutgoingProcessor>([this](auto value) { send(std::move(value)); })},
    m_socket(std::move(webSocket))
{
    base_type::bindToAioThread(m_socket->getAioThread());
    if (auto socket = m_socket->socket())
        m_address = socket->getForeignAddress();
}

void WebSocketConnection::setRequestHandler(RequestHandler requestHandler)
{
    m_incomingProcessor = std::make_unique<IncomingProcessor>(
        [this, requestHandler = std::move(requestHandler)](const auto& request, auto responseHandler)
        {
            requestHandler(
                request,
                [this, handler = std::move(responseHandler)](auto response) mutable
                {
                    dispatch([response = std::move(response), handler = std::move(handler)]() mutable
                    {
                        handler(std::move(response));
                    });
                },
                this);
        });
}

void WebSocketConnection::start()
{
    m_socket->start();
    readNextMessage();
}

WebSocketConnection::~WebSocketConnection()
{
    if (m_onDone)
        m_onDone(SystemError::noError, this);
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
        [this,
            request = std::move(request),
            handler = std::move(handler),
            serializedRequest = std::move(serializedRequest)]() mutable
        {
            m_outgoingProcessor->processRequest(
                std::move(request), std::move(handler), std::move(serializedRequest));
        });
}

void WebSocketConnection::send(
    std::vector<Request> jsonRpcRequests, BatchResponseHandler handler)
{
    dispatch(
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
                nx::utils::moveAndCallOptional(m_onDone, errorCode, this);
                return;
            }

            readHandler(*buffer.get());
            readNextMessage();
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
        [this](std::string response)
        {
            if (!response.empty())
                send(std::move(response));
            m_queuedRequests.pop();
            if (!m_queuedRequests.empty())
                processQueuedRequest();
        });
}

void WebSocketConnection::send(std::string data)
{
    auto buffer = std::make_unique<nx::Buffer>(std::move(data));
    auto bufferPtr = buffer.get();
    logMessage("send to", m_address, *bufferPtr);
    m_socket->sendAsync(bufferPtr,
        [buffer = std::move(buffer), this](auto errorCode, auto /*bytesTransferred*/)
        {
            if (errorCode != SystemError::noError)
            {
                NX_DEBUG(this, "Failed to send message with error code %1", errorCode);
                m_outgoingProcessor->clear(errorCode);
                nx::utils::moveAndCallOptional(m_onDone, errorCode, this);
            }
        });
}

void WebSocketConnection::addGuard(const QString& id, nx::utils::Guard guard)
{
    if (!NX_ASSERT(guard))
        return;

    dispatch(
        [this, id, g = std::move(guard)]() mutable
        {
            auto& guard = m_guards[id];
            NX_VERBOSE(this, "%1 guard for %2", guard ? "Adding" : "Replacing", id);
            guard = std::move(g);
        });
}

void WebSocketConnection::removeGuard(const QString& id)
{
    dispatch(
        [this, id]() mutable
        {
            NX_VERBOSE(this, "Removing guard for %1", id);
            m_guards.erase(id);
        });
}

} // namespace nx::json_rpc
