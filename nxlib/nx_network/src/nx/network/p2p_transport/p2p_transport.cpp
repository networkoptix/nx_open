#include "p2p_transport.h"

namespace nx::network {

P2PTransport::P2PTransport(std::unique_ptr<detail::IP2PSocketDelegate> transportDelegate):
    m_transportDelegate(std::move(transportDelegate))
{
}

void P2PTransport::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{
    m_transportDelegate->readSomeAsync(buffer, std::move(handler));
}

void P2PTransport::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    m_transportDelegate->sendAsync(buffer, std::move(handler));
}

void P2PTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_transportDelegate->cancelIoInAioThread(eventType);
}

void P2PTransport::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_transportDelegate->bindToAioThread(aioThread);
}

aio::AbstractAioThread* P2PTransport::getAioThread() const
{
    return m_transportDelegate->getAioThread();
}

void P2PTransport::pleaseStopSync()
{
    return m_transportDelegate->pleaseStopSync();
}

SocketAddress P2PTransport::getForeignAddress() const
{
    return m_transportDelegate->getForeignAddress();
}

} // namespace nx::network