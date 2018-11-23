#include "p2p_http_client_transport.h"

namespace nx::network {

P2PHttpClientTransport::P2PHttpClientTransport(
    HttpClientPtr readHttpClient,
    nx::utils::MoveOnlyFunc<void()> onPostConnectionEstablished,
    websocket::FrameType messageType)
    :
    m_readHttpClient(std::move(readHttpClient)),
    m_messageType(messageType)
{
}

void P2PHttpClientTransport::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{
}

void P2PHttpClientTransport::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
}

void P2PHttpClientTransport::bindToAioThread(aio::AbstractAioThread* aioThread)
{
}

void P2PHttpClientTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
}

aio::AbstractAioThread* P2PHttpClientTransport::getAioThread() const
{
    return nullptr;
}

void P2PHttpClientTransport::pleaseStopSync()
{
}

SocketAddress P2PHttpClientTransport::getForeignAddress() const
{
    return SocketAddress();
}

} // namespace nx::network
