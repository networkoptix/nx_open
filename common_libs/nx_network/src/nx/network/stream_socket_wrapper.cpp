
#include "stream_socket_wrapper.h"

#include <nx/utils/future.h>
#include <nx/utils/thread/wait_condition.h>


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

bool StreamSocketWrapper::close()
{ return m_socket->close(); }

bool StreamSocketWrapper::shutdown()
{ return m_socket->shutdown(); }

bool StreamSocketWrapper::isClosed() const
{ return m_socket->isClosed(); }

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

bool StreamSocketWrapper::connect(const SocketAddress& sa, unsigned int ms)
{
    NX_ASSERT(false, Q_FUNC_INFO, "Not supported");
    return false;
}

int StreamSocketWrapper::recv(void* buffer, unsigned int bufferLen, int flags)
{
    if (m_nonBlockingMode)
        return m_socket->recv(buffer, bufferLen, flags);

    Buffer recvBuffer;
    recvBuffer.reserve(static_cast<int>(bufferLen));

    nx::utils::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    readSomeAsync(&recvBuffer, [this, &promise](
        SystemError::ErrorCode code, size_t size)
    {
        // need to post here to be sure IO thread is done with this socket
        post([this, &promise, code, size]()
        {
            promise.set_value(std::make_pair(code, size));
        });
    });

    const auto result = promise.get_future().get();
    if (result.first != SystemError::noError)
        return -1;

    std::memcpy(buffer, recvBuffer.data(), result.second);
    return static_cast<int>(result.second);
}

int StreamSocketWrapper::send(const void* buffer, unsigned int bufferLen)
{
    if (m_nonBlockingMode)
        return m_socket->send(buffer, bufferLen);

    Buffer sendBuffer(static_cast<const char*>(buffer), bufferLen);
    nx::utils::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    sendAsync(sendBuffer, [this, &promise](
        SystemError::ErrorCode code, size_t size)
    {
        // need to post here to be sure IO thread is done with this socket
        post([this, &promise, code, size]()
        {
            promise.set_value(std::make_pair(code, size));
        });
    });

    const auto result = promise.get_future().get();
    if (result.first != SystemError::noError)
        return -1;

    return static_cast<int>(result.second);
}

SocketAddress StreamSocketWrapper::getForeignAddress() const
{ return m_socket->getForeignAddress(); }

bool StreamSocketWrapper::isConnected() const
{ return m_socket->isConnected(); }

void StreamSocketWrapper::connectAsync(
    const SocketAddress& /*addr*/,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> /*handler*/)
{
    NX_ASSERT(false, Q_FUNC_INFO, "Not supported");
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
    std::chrono::milliseconds timeoutMs, nx::utils::MoveOnlyFunc<void()> handler)
{ m_socket->registerTimer(timeoutMs, std::move(handler)); }

void StreamSocketWrapper::cancelIOAsync(
    aio::EventType eventType, nx::utils::MoveOnlyFunc<void()> handler)
{ m_socket->cancelIOAsync(eventType, std::move(handler)); }

void StreamSocketWrapper::cancelIOSync(aio::EventType eventType)
{ m_socket->cancelIOSync(eventType); }

bool StreamSocketWrapper::reopen()
{ return m_socket->reopen(); }

void StreamSocketWrapper::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{ return m_socket->pleaseStop(std::move(handler)); }

void StreamSocketWrapper::post(nx::utils::MoveOnlyFunc<void()> handler)
{ return m_socket->post(std::move(handler)); }

void StreamSocketWrapper::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{ return m_socket->dispatch(std::move(handler)); }

} // namespace network
} // namespace nx
