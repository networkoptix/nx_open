#include "p2p_http_server_transport.h"
#include <nx/network/http/http_async_client.h>

namespace nx::network {

P2PHttpServerTransport::P2PHttpServerTransport(std::unique_ptr<AbstractStreamSocket> socket,
    websocket::FrameType messageType)
    :
    m_messageType(messageType)
{
}

void P2PHttpServerTransport::gotPostConnection(std::unique_ptr<AbstractStreamSocket> socket)
{
}

void P2PHttpServerTransport::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{
}

void P2PHttpServerTransport::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
}

void P2PHttpServerTransport::bindToAioThread(aio::AbstractAioThread* aioThread)
{
}

void P2PHttpServerTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
}

aio::AbstractAioThread* P2PHttpServerTransport::getAioThread() const
{
    return nullptr;
}

void P2PHttpServerTransport::pleaseStopSync()
{
}

SocketAddress P2PHttpServerTransport::getForeignAddress() const
{
    return SocketAddress();
}

} // namespace nx::network
