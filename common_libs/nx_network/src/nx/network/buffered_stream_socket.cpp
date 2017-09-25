#include "buffered_stream_socket.h"

#include <cstring>

#include <nx/utils/log/log.h>

static const int kRecvBufferCapacity = 1024 * 4;

namespace nx {
namespace network {

BufferedStreamSocket::BufferedStreamSocket(std::unique_ptr<AbstractStreamSocket> socket):
    StreamSocketDelegate(socket.get()),
    m_socket(std::move(socket))
{
}

void BufferedStreamSocket::catchRecvEvent(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_catchRecvEventHandler = std::move(handler);

    if (!m_internalRecvBuffer.isEmpty())
        return triggerCatchRecvEvent(SystemError::noError);

    if (m_internalRecvBuffer.capacity() < kRecvBufferCapacity)
        m_internalRecvBuffer.reserve(kRecvBufferCapacity);

    m_socket->readSomeAsync(
        &m_internalRecvBuffer,
        [this](SystemError::ErrorCode code, size_t size) mutable
        {
            NX_LOGX(lm("catchRecvEvent read size=%1: %2")
                .arg(code == SystemError::noError ? size : 0)
                .arg(SystemError::toString(code)), cl_logDEBUG2);

            // Here we treat connection closure as an error:
            if (code == SystemError::noError && size == 0)
                code = SystemError::connectionReset;

            // Make socket ready for blocking mode:
            triggerCatchRecvEvent(code);
        });
}

void BufferedStreamSocket::injectRecvData(Buffer buffer, Inject injectType)
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

int BufferedStreamSocket::recv(void* buffer, unsigned int bufferLen, int flags)
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

void BufferedStreamSocket::readSomeAsync(
    nx::Buffer* const buf,
    IoCompletionHandler handler)
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

QString BufferedStreamSocket::idForToStringFromPtr() const
{
    return toString(m_socket);
}

void BufferedStreamSocket::triggerCatchRecvEvent(SystemError::ErrorCode resultCode)
{
    post(
        [this, resultCode]()
        {
            nx::utils::swapAndCall(m_catchRecvEventHandler, resultCode);
        });
}

} // namespace network
} // namespace nx
