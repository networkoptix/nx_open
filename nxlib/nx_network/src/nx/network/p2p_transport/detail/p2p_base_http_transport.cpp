#include "p2p_base_http_transport.h"

namespace nx::network::detail {

P2PBaseHttpTransport::P2PBaseHttpTransport(std::unique_ptr<AbstractStreamSocket> streamSocket,
    websocket::FrameType frameType)
{
}

void P2PBaseHttpTransport::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{
}

void P2PBaseHttpTransport::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
}

void P2PBaseHttpTransport::bindToAioThread(aio::AbstractAioThread* aioThread)
{
}

void P2PBaseHttpTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
}

aio::AbstractAioThread* P2PBaseHttpTransport::getAioThread() const
{
    return nullptr;
}

void P2PBaseHttpTransport::pleaseStopSync()
{
}

SocketAddress P2PBaseHttpTransport::getForeignAddress() const
{
    return SocketAddress();
}

} // namespace nx::network::detail