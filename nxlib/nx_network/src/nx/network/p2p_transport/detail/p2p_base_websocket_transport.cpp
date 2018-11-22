#include "p2p_base_websocket_transport.h"

namespace nx::network::detail {

P2BaseWebsocketTransport::P2BaseWebsocketTransport(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    websocket::FrameType frameType)
    :
    m_webSocket(new WebSocket(std::move(streamSocket), frameType))
{
    // TODO: #akulikov change that to real value
    m_webSocket->setAliveTimeout(std::chrono::seconds(60));
    m_webSocket->start();
}

void P2BaseWebsocketTransport::readSomeAsync(nx::Buffer* const buffer,
    IoCompletionHandler handler)
{
    m_webSocket->readSomeAsync(buffer, std::move(handler));
}

void P2BaseWebsocketTransport::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    m_webSocket->sendAsync(buffer, std::move(handler));
}

void P2BaseWebsocketTransport::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_webSocket->bindToAioThread(aioThread);
}

void P2BaseWebsocketTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_webSocket->cancelIoInAioThread(eventType);
}

aio::AbstractAioThread* P2BaseWebsocketTransport::getAioThread() const
{
    return m_webSocket->getAioThread();
}

void P2BaseWebsocketTransport::pleaseStopSync()
{
    m_webSocket->pleaseStopSync();
}

SocketAddress P2BaseWebsocketTransport::getForeignAddress() const
{
    return m_webSocket->socket()->getForeignAddress();
}

} // namespace nx::network::detail