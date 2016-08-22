#include "extended_stream_socket.h"

#include <cstring>

#include <nx/utils/log/log.h>

static const int kRecvBufferCapacity = 1024 * 4;

namespace nx {
namespace network {

ExtendedStreamSocket::ExtendedStreamSocket(std::unique_ptr<AbstractStreamSocket> socket)
    : m_socket(std::move(socket))
{
    setDelegate(m_socket.get());
}

void ExtendedStreamSocket::waitForRecvData(
    std::function<void(SystemError::ErrorCode)> handler)
{
    if (!m_internalRecvBuffer.isEmpty())
        return m_socket->dispatch(std::bind(std::move(handler), SystemError::noError));

    if (m_internalRecvBuffer.capacity() < kRecvBufferCapacity)
        m_internalRecvBuffer.reserve(kRecvBufferCapacity);

    m_socket->readSomeAsync(
        &m_internalRecvBuffer,
        [this, handler = std::move(handler)](SystemError::ErrorCode code, size_t size) mutable
        {
            NX_LOGX(lm("waitForRecvData read size=%1: %2")
                .arg(code == SystemError::noError ? size : 0)
                .arg(SystemError::toString(code)), cl_logDEBUG2);

            // Here we treat connection closure as an error:
            if (code == SystemError::noError && size == 0)
                code = SystemError::connectionReset;

            // Make socket ready for blocking mode:
            m_socket->post(std::bind(std::move(handler), code));
        });
}

void ExtendedStreamSocket::injectRecvData(Buffer buffer, Inject injectType)
{
    NX_LOGX(lm("injectRecvData size=%1, injectType=%2")
        .arg(buffer.size()).arg(static_cast<int>(injectType)), cl_logDEBUG2);

    switch (injectType)
    {
        case Inject::only:
            NX_ASSERT(m_internalRecvBuffer.isEmpty());
            /* fall through */
        case Inject::replace:
            m_internalRecvBuffer = std::move(buffer);
            return;

        case Inject::begin:
            std::swap(m_internalRecvBuffer, buffer);
            /* fall through */
        case Inject::end:
            m_internalRecvBuffer.append(buffer);
            return;
    }

    NX_ASSERT(false, lm("Unexpected enum value: %1").arg((int)injectType));
}

bool ExtendedStreamSocket::bind(const SocketAddress& localAddress)
{
    return m_socket->bind(localAddress);
}

SocketAddress ExtendedStreamSocket::getLocalAddress() const
{
    return m_socket->getLocalAddress();
}

bool ExtendedStreamSocket::close()
{
    return m_socket->close();
}

bool ExtendedStreamSocket::isClosed() const
{
    return m_socket->isClosed();
}

bool ExtendedStreamSocket::shutdown()
{
    return m_socket->shutdown();
}

bool ExtendedStreamSocket::reopen()
{
    return m_socket->reopen();
}

bool ExtendedStreamSocket::connect(
    const SocketAddress& remoteAddress,
    unsigned int timeoutMillis)
{
    return m_socket->connect(remoteAddress, timeoutMillis);
}

int ExtendedStreamSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    if (m_internalRecvBuffer.isEmpty())
        return m_socket->recv(buffer, bufferLen, flags);

    // Drain internal buffer first
    auto internalSize = std::min((unsigned int)m_internalRecvBuffer.size(), bufferLen);
    std::memcpy(buffer, m_internalRecvBuffer.data(), internalSize);
    m_internalRecvBuffer = m_internalRecvBuffer.mid((int)internalSize);

    if (internalSize == bufferLen || flags != MSG_WAITALL)
    {
        NX_LOGX(lm("recv internalSize=%1").arg(internalSize), cl_logDEBUG2);
        return (int)internalSize;
    }

    // In case of unsatisfied MSG_WAITALL, read the rest from real socket
    int recv = m_socket->recv((char*)buffer + internalSize, bufferLen - internalSize, flags);
    NX_LOGX(lm("recv internalSize=%1 + realRecv=%2").arg(internalSize).arg(recv), cl_logDEBUG2);
    return (recv < 0) ? (int)internalSize : internalSize + recv;
}

int ExtendedStreamSocket::send(const void* buffer, unsigned int bufferLen)
{
    return m_socket->send(buffer, bufferLen);
}

SocketAddress ExtendedStreamSocket::getForeignAddress() const
{
    return m_socket->getForeignAddress();
}

bool ExtendedStreamSocket::isConnected() const
{
    return m_socket->isConnected();
}

void ExtendedStreamSocket::cancelIOAsync(
    aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    return m_socket->cancelIOAsync(eventType, std::move(handler));
}

void ExtendedStreamSocket::cancelIOSync(aio::EventType eventType)
{
    return m_socket->cancelIOSync(eventType);
}

void ExtendedStreamSocket::connectAsync(
    const SocketAddress& address,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_socket->connectAsync(address, std::move(handler));
}

void ExtendedStreamSocket::readSomeAsync(
    nx::Buffer* const buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_internalRecvBuffer.isEmpty())
        return m_socket->readSomeAsync(buf, std::move(handler));

    // Drain internal buffer
    auto recvSize = std::min(m_internalRecvBuffer.size(), buf->capacity() - buf->size());

    auto oldSize = buf->size();
    buf->resize(oldSize + recvSize);

    std::memcpy(buf->data() + oldSize, m_internalRecvBuffer.data(), recvSize);
    m_internalRecvBuffer = m_internalRecvBuffer.mid(recvSize);

    // Post handler like it the event just happend on the real socket
    m_socket->post(
        [this, recvSize, handler = std::move(handler)]()
        {
            NX_LOGX(lm("readSomeAsync internalSize=%1").arg(recvSize), cl_logDEBUG2);
            handler(SystemError::noError, (size_t)recvSize);
        });
}

void ExtendedStreamSocket::sendAsync(
    const nx::Buffer& buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    m_socket->sendAsync(buf, std::move(handler));
}

} // namespace network
} // namespace nx
