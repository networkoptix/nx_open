#include "p2p_websocket_transport.h"

namespace nx::network {

P2PWebsocketTransport::P2PWebsocketTransport(
    std::unique_ptr<AbstractStreamSocket> socket,
    websocket::FrameType frameType)
    :
    m_webSocket(new WebSocket(std::move(socket), frameType))
{
    // TODO: #akulikov change that to real value
    m_webSocket->setAliveTimeout(std::chrono::seconds(60));
    m_webSocket->start();
}

void P2PWebsocketTransport::readSomeAsync(nx::Buffer* const buffer,
    IoCompletionHandler handler)
{
    m_webSocket->readSomeAsync(buffer, std::move(handler));
}

void P2PWebsocketTransport::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    m_webSocket->sendAsync(buffer, std::move(handler));
}

void P2PWebsocketTransport::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_webSocket->bindToAioThread(aioThread);
}

void P2PWebsocketTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_webSocket->cancelIoInAioThread(eventType);
}

aio::AbstractAioThread* P2PWebsocketTransport::getAioThread() const
{
    return m_webSocket->getAioThread();
}

void P2PWebsocketTransport::pleaseStopSync()
{
    m_webSocket->pleaseStopSync();
}

SocketAddress P2PWebsocketTransport::getForeignAddress() const
{
    return m_webSocket->socket()->getForeignAddress();
}

} // namespace nx::network