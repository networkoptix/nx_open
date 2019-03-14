#include "dummy_socket.h"

#include <nx/utils/random.h>

namespace nx {
namespace network {

DummySocket::DummySocket():
    m_localAddress(HostAddress::localhost, nx::utils::random::number<quint16>(5000, 50000))
{
}

bool DummySocket::bind(const SocketAddress& localAddress)
{
    m_localAddress = localAddress;
    return true;
}

SocketAddress DummySocket::getLocalAddress() const
{
    return m_localAddress;
}

bool DummySocket::setReuseAddrFlag(bool /*reuseAddr*/)
{
    return true;
}

bool DummySocket::getReuseAddrFlag(bool* /*val*/) const
{
    return true;
}

bool DummySocket::setReusePortFlag(bool /*value*/)
{
    return true;
}

bool DummySocket::getReusePortFlag(bool* /*value*/) const
{
    return true;
}

bool DummySocket::setNonBlockingMode(bool /*val*/)
{
    return true;
}

bool DummySocket::getNonBlockingMode(bool* /*val*/) const
{
    return true;
}

bool DummySocket::getMtu(unsigned int* mtuValue) const
{
    *mtuValue = 1500;
    return true;
}

bool DummySocket::setSendBufferSize(unsigned int /*buffSize*/)
{
    return true;
}

bool DummySocket::getSendBufferSize(unsigned int* buffSize) const
{
    *buffSize = 8192;
    return true;
}

bool DummySocket::setRecvBufferSize(unsigned int /*buffSize*/)
{
    return true;
}

bool DummySocket::getRecvBufferSize(unsigned int* buffSize) const
{
    *buffSize = 8192;
    return true;
}

bool DummySocket::setRecvTimeout(unsigned int /*millis*/)
{
    return true;
}

bool DummySocket::getRecvTimeout(unsigned int* millis) const
{
    *millis = 0;
    return true;
}

bool DummySocket::setSendTimeout(unsigned int /*ms*/)
{
    return true;
}

bool DummySocket::getSendTimeout(unsigned int* millis) const
{
    *millis = 0;
    return true;
}

bool DummySocket::getLastError(SystemError::ErrorCode* /*errorCode*/) const
{
    return false;
}

bool DummySocket::setIpv6Only(bool /*val*/)
{
    return false;
}

bool DummySocket::getProtocol(int* /*protocol*/) const
{
    return false;
}

AbstractSocket::SOCKET_HANDLE DummySocket::handle() const
{
    return m_basicPollable.pollable().handle();
}

Pollable* DummySocket::pollable()
{
    return &m_basicPollable.pollable();
}

bool DummySocket::connect(
    const SocketAddress& remoteSocketAddress,
    std::chrono::milliseconds /*timeout*/)
{
    m_remotePeerAddress = remoteSocketAddress;
    return true;
}

SocketAddress DummySocket::getForeignAddress() const
{
    return m_remotePeerAddress;
}

void DummySocket::cancelIoInAioThread(aio::EventType /*eventType*/)
{
}

bool DummySocket::setNoDelay(bool /*value*/)
{
    return true;
}

bool DummySocket::getNoDelay(bool* /*value*/) const
{
    return true;
}

bool DummySocket::toggleStatisticsCollection(bool /*val*/)
{
    return false;
}

bool DummySocket::getConnectionStatistics(StreamSocketInfo* /*info*/)
{
    return false;
}

bool DummySocket::setKeepAlive(std::optional< KeepAliveOptions > /*info*/)
{
    return true;
}

bool DummySocket::getKeepAlive(std::optional< KeepAliveOptions >* /*result*/) const
{
    return false;
}

void DummySocket::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_basicPollable.post(std::move(handler));
}

void DummySocket::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_basicPollable.dispatch(std::move(handler));
}

void DummySocket::connectAsync(
    const SocketAddress& /*addr*/,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    post([handler = std::move(handler)]() { handler(SystemError::notImplemented); });
}

void DummySocket::readSomeAsync(
    nx::Buffer* const /*buf*/,
    IoCompletionHandler handler)
{
    post([handler = std::move(handler)]() { handler(SystemError::notImplemented, (size_t)-1); });
}

void DummySocket::sendAsync(
    const nx::Buffer& /*buf*/,
    IoCompletionHandler handler)
{
    post([handler = std::move(handler)]() { handler(SystemError::notImplemented, (size_t)-1); });
}

void DummySocket::registerTimer(
    std::chrono::milliseconds /*timeoutMs*/,
    nx::utils::MoveOnlyFunc<void()> /*handler*/)
{
    NX_ASSERT(false);
}

aio::AbstractAioThread* DummySocket::getAioThread() const
{
    return m_basicPollable.getAioThread();
}

void DummySocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    return m_basicPollable.bindToAioThread(aioThread);
}

} // namespace network
} // namespace nx
