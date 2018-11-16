#include "p2p_transport_websocket_delegate.h"

namespace nx::network::detail {

P2TransportWebsocketDelegate::P2TransportWebsocketDelegate(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    websocket::FrameType frameType)
    :
    m_webSocket(new WebSocket(std::move(streamSocket), frameType))
{
    // TODO: #akulikov change that to real value
    m_webSocket->setAliveTimeout(std::chrono::seconds(60));
    m_webSocket->start();
}

void P2TransportWebsocketDelegate::readSomeAsync(nx::Buffer* const buffer,
    IoCompletionHandler handler)
{
    m_webSocket->readSomeAsync(buffer, std::move(handler));
}

void P2TransportWebsocketDelegate::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    m_webSocket->sendAsync(buffer, std::move(handler));
}

void P2TransportWebsocketDelegate::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_webSocket->bindToAioThread(aioThread);
}

void P2TransportWebsocketDelegate::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_webSocket->cancelIoInAioThread(eventType);
}

aio::AbstractAioThread* P2TransportWebsocketDelegate::getAioThread() const
{
    return m_webSocket->getAioThread();
}

void P2TransportWebsocketDelegate::pleaseStopSync()
{
    m_webSocket->pleaseStopSync();
}

SocketAddress P2TransportWebsocketDelegate::getForeignAddress() const
{
    return m_webSocket->socket()->getForeignAddress();
}

} // namespace nx::network::detail