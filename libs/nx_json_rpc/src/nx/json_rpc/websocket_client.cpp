// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_client.h"

#include <nx/network/http/http_async_client.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/utils/scope_guard.h>

#include "websocket_connection.h"

namespace nx::json_rpc {

WebSocketClient::WebSocketClient(
    nx::utils::Url url,
    std::unique_ptr<nx::network::http::AsyncClient> client,
    RequestHandler handler)
    :
    m_url(std::move(url)),
    m_handler(std::move(handler)),
    m_handshakeClient(std::move(client))
{
    nx::network::http::HttpHeaders httpHeaders;
    nx::network::websocket::addClientHeaders(&httpHeaders,
        nx::network::websocket::kWebsocketProtocolName,
        nx::network::websocket::CompressionType::perMessageDeflate);
    for (const auto& [header, value]: httpHeaders)
        m_handshakeClient->addAdditionalHeader(header, value);
    base_type::bindToAioThread(m_handshakeClient->getAioThread());
}

WebSocketClient::WebSocketClient(
    nx::utils::Url url,
    nx::network::http::Credentials credentials,
    nx::network::ssl::AdapterFunc adapterFunc,
    RequestHandler handler)
    :
    WebSocketClient(
        std::move(url),
        [](nx::network::http::Credentials credentials, nx::network::ssl::AdapterFunc adapterFunc)
        {
            auto r = std::make_unique<nx::network::http::AsyncClient>(std::move(adapterFunc));
            r->setCredentials(std::move(credentials));
            return r;
        }(std::move(credentials), std::move(adapterFunc)),
    std::move(handler))
{
}

WebSocketClient::~WebSocketClient()
{
    pleaseStopSync();
}

void WebSocketClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_connection)
        m_connection->bindToAioThread(aioThread);
    m_handshakeClient->bindToAioThread(aioThread);
}

void WebSocketClient::setHandler(RequestHandler handler)
{
    NX_ASSERT(!m_handler);
    NX_ASSERT(!m_connection);
    m_handler = std::move(handler);
}

void WebSocketClient::connectAsync(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    post(
        [this, handler = std::move(handler)]() mutable
        {
            m_connectHandlers.push_back(std::move(handler));
            if (m_connectHandlers.size() != 1)
                return;

            m_handshakeClient->doUpgrade(
                m_url, nx::network::websocket::kWebsocketProtocolName, [this]() { onUpgrade(); });
        });
}

void WebSocketClient::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_connection.reset();
    m_handshakeClient->pleaseStopSync();
    auto handlers = std::move(m_connectHandlers);
    for (const auto& handler: handlers)
        handler(SystemError::interrupted);
}

void WebSocketClient::onUpgrade()
{
    SystemError::ErrorCode errorCode = SystemError::noError;
    nx::utils::ScopeGuard guard(
        [&errorCode, handlers = std::move(m_connectHandlers)]()
        {
            for (const auto& handler: handlers)
                handler(errorCode);
        });

    auto socket = m_handshakeClient->takeSocket();
    if (!socket)
    {
        errorCode = m_handshakeClient->lastSysErrorCode();
        NX_DEBUG(
            this, "Connect to %1 is failed with error %2", m_handshakeClient->url(), errorCode);
        return;
    }

    socket->setNonBlockingMode(true);
    socket->bindToAioThread(getAioThread());
    m_connection = std::make_unique<WebSocketConnection>(
        std::make_unique<nx::network::websocket::WebSocket>(std::move(socket),
            nx::network::websocket::Role::client,
            nx::network::websocket::FrameType::text,
            nx::network::websocket::CompressionType::perMessageDeflate),
        [this](auto connection)
        {
            if (m_connection)
            {
                NX_ASSERT(m_connection.get() == connection,
                    "m_connection=%1, connection=%2", m_connection, connection);
                auto connectionPtr = m_connection.get();
                connectionPtr->pleaseStop([connection = std::move(m_connection)]() {});
            }
        });
    if (m_handler)
    {
        m_connection->setRequestHandler(
            [this](auto&&... args) { m_handler(std::forward<decltype(args)>(args)...); });
    }
    m_connection->start();
}

void WebSocketClient::sendAsync(Request request, ResponseHandler handler)
{
    post(
        [this, request = std::move(request), handler = std::move(handler)]() mutable
        {
            if (m_connection)
            {
                m_connection->send(std::move(request), std::move(handler));
                return;
            }

            connectAsync(
                [this, request = std::move(request), handler = std::move(handler)](
                    auto errorCode) mutable
                {
                    if (m_connection)
                    {
                        m_connection->send(std::move(request), std::move(handler));
                        return;
                    }

                    handler(Response::makeError(request.responseId(),
                        {Error::transportError,
                            NX_FMT("Failed to establish connnection to %1 with error %2",
                                m_url, errorCode).toStdString()}));
                });
        });
}

} // namespace nx::json_rpc
