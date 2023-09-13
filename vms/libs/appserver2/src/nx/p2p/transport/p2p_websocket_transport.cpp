// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "p2p_websocket_transport.h"

namespace nx::p2p {

P2PWebsocketTransport::P2PWebsocketTransport(
    std::unique_ptr<network::AbstractStreamSocket> socket,
    network::websocket::Role role,
    network::websocket::FrameType frameType,
    network::websocket::CompressionType compressionType,
    std::chrono::milliseconds aliveTimeout)
    :
    m_webSocket(new network::WebSocket(std::move(socket), role, frameType, compressionType))
{
    bindToAioThread(m_webSocket->getAioThread());
    m_webSocket->setAliveTimeout(aliveTimeout);
}

void P2PWebsocketTransport::start(utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart)
{
    m_webSocket->start();
    if (onStart)
        post([onStart = std::move(onStart)] { onStart(SystemError::noError); });
}

void P2PWebsocketTransport::readSomeAsync(
    nx::Buffer* const buffer,
    network::IoCompletionHandler handler)
{
    m_webSocket->readSomeAsync(buffer, std::move(handler));
}

void P2PWebsocketTransport::sendAsync(
    const nx::Buffer* buffer,
    network::IoCompletionHandler handler)
{
    m_webSocket->sendAsync(buffer, std::move(handler));
}

void P2PWebsocketTransport::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    BasicPollable::bindToAioThread(aioThread);
    m_webSocket->bindToAioThread(aioThread);
}

void P2PWebsocketTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_webSocket->cancelIoInAioThread(eventType);
}

network::aio::AbstractAioThread* P2PWebsocketTransport::getAioThread() const
{
    return m_webSocket->getAioThread();
}

void P2PWebsocketTransport::pleaseStopSync()
{
    m_webSocket->pleaseStopSync();
}

network::SocketAddress P2PWebsocketTransport::getForeignAddress() const
{
    return m_webSocket->socket()->getForeignAddress();
}

} // namespace nx::network
