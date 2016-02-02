#include "stream_socket_wrapper.h"

#include "nx/utils/thread/wait_condition.h"

namespace nx {
namespace network {

StreamSocketWrapper::StreamSocketWrapper(std::unique_ptr<AbstractStreamSocket> socket)
    : m_nonBlockingMode(false)
    , m_socket(std::move(socket))
{
}

bool StreamSocketWrapper::bind(const SocketAddress& localAddress)
{ return m_socket->bind(localAddress); }

SocketAddress StreamSocketWrapper::getLocalAddress() const
{ return m_socket->getLocalAddress(); }

void StreamSocketWrapper::close()
{ m_socket->close(); }

void StreamSocketWrapper::shutdown()
{ m_socket->shutdown(); }

bool StreamSocketWrapper::isClosed() const
{ return m_socket->isClosed(); }

bool StreamSocketWrapper::setReuseAddrFlag(bool reuseAddr)
{ return m_socket->setReuseAddrFlag(reuseAddr); }

bool StreamSocketWrapper::getReuseAddrFlag(bool* val) const
{ return m_socket->getReuseAddrFlag(val); }

bool StreamSocketWrapper::setNonBlockingMode(bool val)
{
    m_nonBlockingMode = val;
    return true;
}

bool StreamSocketWrapper::getNonBlockingMode(bool* val) const
{
    *val = m_nonBlockingMode;
    return true;
}

bool StreamSocketWrapper::getMtu(unsigned int* mtuValue) const
{ return m_socket->getMtu(mtuValue); }

bool StreamSocketWrapper::setSendBufferSize(unsigned int buffSize)
{ return m_socket->setSendBufferSize(buffSize); }

bool StreamSocketWrapper::getSendBufferSize(unsigned int* buffSize) const
{ return m_socket->getSendBufferSize(buffSize); }

bool StreamSocketWrapper::setRecvBufferSize(unsigned int buffSize)
{ return m_socket->setRecvBufferSize(buffSize); }

bool StreamSocketWrapper::getRecvBufferSize(unsigned int* buffSize) const
{ return m_socket->getRecvBufferSize(buffSize); }

bool StreamSocketWrapper::setRecvTimeout(unsigned int millis)
{ return m_socket->setRecvTimeout(millis); }

bool StreamSocketWrapper::getRecvTimeout(unsigned int* millis) const
{ return m_socket->getRecvTimeout(millis); }

bool StreamSocketWrapper::setSendTimeout(unsigned int ms)
{ return m_socket->setSendTimeout(ms); }

bool StreamSocketWrapper::getSendTimeout(unsigned int* millis) const
{ return m_socket->getSendTimeout(millis); }

bool StreamSocketWrapper::getLastError(SystemError::ErrorCode* errorCode) const
{ return m_socket->getLastError(errorCode); }

AbstractSocket::SOCKET_HANDLE StreamSocketWrapper::handle() const
{ return m_socket->handle(); }

aio::AbstractAioThread* StreamSocketWrapper::getAioThread()
{ return m_socket->getAioThread(); }

void StreamSocketWrapper::bindToAioThread(aio::AbstractAioThread* aioThread)
{ return m_socket->bindToAioThread(aioThread); }

bool StreamSocketWrapper::connect(const SocketAddress& sa, unsigned int ms)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Not supported");
}

int StreamSocketWrapper::recv(void* buffer, unsigned int bufferLen, int flags)
{
    if (m_nonBlockingMode)
        return m_socket->recv(buffer, bufferLen, flags);

    Buffer recvBuffer;
    recvBuffer.reserve(static_cast<int>(bufferLen));

    std::promise<size_t> result;
    readSomeAsync(&recvBuffer, [this, &result](
        SystemError::ErrorCode code, size_t size)
    {
        // need to post here to be sure IO thread is done with this socket
        post([this, &result, size]() { result.set_value(size); });
    });

    size_t size = result.get_future().get();
    std::memcpy(buffer, recvBuffer.data(), size);
    return static_cast<int>(size);
}

int StreamSocketWrapper::send(const void* buffer, unsigned int bufferLen)
{
    if (m_nonBlockingMode)
        return m_socket->send(buffer, bufferLen);

    Buffer sendBuffer(static_cast<const char*>(buffer), bufferLen);
    std::promise<size_t> result;
    sendAsync(sendBuffer, [this, &result](
        SystemError::ErrorCode code, size_t size)
    {
        // need to post here to be sure IO thread is done with this socket
        post([this, &result, size]() { result.set_value(size); });
    });

    return static_cast<int>(result.get_future().get());
}

SocketAddress StreamSocketWrapper::getForeignAddress() const
{ return m_socket->getForeignAddress(); }

bool StreamSocketWrapper::isConnected() const
{ return m_socket->isConnected(); }

void StreamSocketWrapper::connectAsync(
    const SocketAddress& /*addr*/,
    std::function<void(SystemError::ErrorCode)> /*handler*/)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Not supported");
}

void StreamSocketWrapper::readSomeAsync(
    nx::Buffer* const buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{ m_socket->readSomeAsync(buf, std::move(handler)); }

void StreamSocketWrapper::sendAsync(
    const nx::Buffer& buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{ m_socket->sendAsync(buf, std::move(handler)); }

void StreamSocketWrapper::registerTimer(
    unsigned int timeoutMs, std::function<void()> handler)
{ m_socket->registerTimer(timeoutMs, std::move(handler)); }

void StreamSocketWrapper::cancelIOAsync(
    aio::EventType eventType, std::function<void()> handler)
{ m_socket->cancelIOAsync(eventType, std::move(handler)); }

void StreamSocketWrapper::cancelIOSync(aio::EventType eventType)
{ m_socket->cancelIOSync(eventType); }

bool StreamSocketWrapper::reopen()
{ return m_socket->reopen(); }

bool StreamSocketWrapper::setNoDelay(bool value)
{ return m_socket->setNoDelay(value); }

bool StreamSocketWrapper::getNoDelay(bool* value) const
{ return m_socket->getNoDelay(value); }

bool StreamSocketWrapper::toggleStatisticsCollection(bool val)
{ return m_socket->toggleStatisticsCollection(val); }

bool StreamSocketWrapper::getConnectionStatistics(StreamSocketInfo* info)
{ return m_socket->getConnectionStatistics(info); }

bool StreamSocketWrapper::setKeepAlive(boost::optional<KeepAliveOptions> info)
{ return m_socket->setKeepAlive(info); }

bool StreamSocketWrapper::getKeepAlive(boost::optional<KeepAliveOptions>* result) const
{ return m_socket->getKeepAlive(result); }

void StreamSocketWrapper::pleaseStop(std::function<void()> handler)
{ return m_socket->pleaseStop(std::move(handler)); }

void StreamSocketWrapper::post(std::function<void()> handler)
{ return m_socket->post(std::move(handler)); }

void StreamSocketWrapper::dispatch(std::function<void()> handler)
{ return m_socket->dispatch(std::move(handler)); }

} // namespace network
} // namespace nx
