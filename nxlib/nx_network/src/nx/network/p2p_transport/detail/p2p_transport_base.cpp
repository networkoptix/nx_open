#include "p2p_transport_base.h"

namespace nx::network::detail {

P2PTransportBase::P2PTransportBase(std::unique_ptr<detail::IP2PSocketDelegate> transportDelegate):
    m_transportDelegate(std::move(transportDelegate))
{
}

void P2PTransportBase::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{
    m_transportDelegate->readSomeAsync(buffer, std::move(handler));
}

void P2PTransportBase::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    m_transportDelegate->sendAsync(buffer, std::move(handler));
}

void P2PTransportBase::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_transportDelegate->cancelIoInAioThread(eventType);
}

void P2PTransportBase::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_transportDelegate->bindToAioThread(aioThread);
}

aio::AbstractAioThread* P2PTransportBase::getAioThread() const
{
    return m_transportDelegate->getAioThread();
}

void P2PTransportBase::pleaseStopSync()
{
    return m_transportDelegate->pleaseStopSync();
}

SocketAddress P2PTransportBase::getForeignAddress() const
{
    return m_transportDelegate->getForeignAddress();
}

} // namespace nx::network::detail