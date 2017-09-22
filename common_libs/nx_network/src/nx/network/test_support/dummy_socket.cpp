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

AbstractSocket::SOCKET_HANDLE DummySocket::handle() const
{
    return 0;
}

Pollable* DummySocket::pollable()
{
    return nullptr;
}

bool DummySocket::connect(
    const SocketAddress& remoteSocketAddress,
    unsigned int /*timeoutMillis*/)
{
    m_remotePeerAddress = remoteSocketAddress;
    return true;
}

SocketAddress DummySocket::getForeignAddress() const
{
    return m_remotePeerAddress;
}

void DummySocket::cancelIOAsync(
    aio::EventType /*eventType*/,
    nx::utils::MoveOnlyFunc< void() > cancellationDoneHandler)
{
    cancellationDoneHandler();
}

void DummySocket::cancelIOSync(aio::EventType /*eventType*/)
{
}

bool DummySocket::reopen()
{
    return connect(m_remotePeerAddress);
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

bool DummySocket::setKeepAlive(boost::optional< KeepAliveOptions > /*info*/)
{
    return true;
}

bool DummySocket::getKeepAlive(boost::optional< KeepAliveOptions >* /*result*/) const
{
    return false;
}

void DummySocket::post(nx::utils::MoveOnlyFunc<void()> /*handler*/)
{
}

void DummySocket::dispatch(nx::utils::MoveOnlyFunc<void()> /*handler*/)
{
}

void DummySocket::connectAsync(
    const SocketAddress& /*addr*/,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> /*handler*/)
{
}

void DummySocket::readSomeAsync(
    nx::Buffer* const /*buf*/,
    IoCompletionHandler /*handler*/)
{
}

void DummySocket::sendAsync(
    const nx::Buffer& /*buf*/,
    IoCompletionHandler /*handler*/)
{
}

void DummySocket::registerTimer(
    std::chrono::milliseconds /*timeoutMs*/,
    nx::utils::MoveOnlyFunc<void()> /*handler*/)
{
}

aio::AbstractAioThread* DummySocket::getAioThread() const
{
    return nullptr;
}

void DummySocket::bindToAioThread(aio::AbstractAioThread* /*aioThread*/)
{
}

} // namespace network
} // namespace nx
