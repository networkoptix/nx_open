// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buffered_stream_socket.h"

#include <cstring>

#include <nx/utils/log/log.h>

static constexpr int kRecvBufferCapacity = 1024 * 4;

namespace nx::network {

BufferedStreamSocket::BufferedStreamSocket(
    std::unique_ptr<AbstractStreamSocket> socket,
    nx::Buffer preReadData)
    :
    StreamSocketDelegate(socket.get()),
    m_socket(std::move(socket)),
    m_internalRecvBuffer(std::move(preReadData))
{
}

void BufferedStreamSocket::catchRecvEvent(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_catchRecvEventHandler = std::move(handler);

    if (!m_internalRecvBuffer.empty())
        return triggerCatchRecvEvent(SystemError::noError);

    if (m_internalRecvBuffer.capacity() < kRecvBufferCapacity)
        m_internalRecvBuffer.reserve(kRecvBufferCapacity);

    m_socket->readSomeAsync(
        &m_internalRecvBuffer,
        [this](SystemError::ErrorCode code, size_t size) mutable
        {
            NX_VERBOSE(this, nx::format("catchRecvEvent read size=%1: %2")
                .arg(code == SystemError::noError ? size : 0)
                .arg(SystemError::toString(code)));

            // Here we treat connection closure as an error:
            if (code == SystemError::noError && size == 0)
                code = SystemError::connectionReset;

            // Make socket ready for blocking mode:
            triggerCatchRecvEvent(code);
        });
}

int BufferedStreamSocket::recv(void* buffer, std::size_t bufferLen, int flags)
{
    if (m_internalRecvBuffer.empty())
        return m_socket->recv(buffer, bufferLen, flags);

    // Drain internal buffer first
    const auto internalSize = std::min(m_internalRecvBuffer.size(), bufferLen);
    std::memcpy(buffer, m_internalRecvBuffer.data(), internalSize);
    m_internalRecvBuffer = m_internalRecvBuffer.substr(internalSize);

    if (internalSize == bufferLen || flags != MSG_WAITALL)
    {
        NX_VERBOSE(this, nx::format("recv internalSize=%1").arg(internalSize));
        return (int) internalSize;
    }

    // In case of unsatisfied MSG_WAITALL, read the rest from real socket
    int recv = m_socket->recv((char*)buffer + internalSize, bufferLen - internalSize, flags);
    NX_VERBOSE(this, nx::format("recv internalSize=%1 + realRecv=%2").arg(internalSize).arg(recv));
    return (int) ((recv < 0) ? internalSize : internalSize + recv);
}

void BufferedStreamSocket::readSomeAsync(
    nx::Buffer* const buf,
    IoCompletionHandler handler)
{
    if (m_internalRecvBuffer.empty())
        return m_socket->readSomeAsync(buf, std::move(handler));

    // Drain internal buffer
    auto recvSize = std::min(m_internalRecvBuffer.size(), buf->capacity() - buf->size());

    auto oldSize = buf->size();
    buf->resize(oldSize + recvSize);

    std::memcpy(buf->data() + oldSize, m_internalRecvBuffer.data(), recvSize);
    m_internalRecvBuffer = m_internalRecvBuffer.substr(recvSize);

    // Post handler like it the event just happend on the real socket
    m_socket->post(
        [this, recvSize, handler = std::move(handler)]()
        {
            NX_VERBOSE(this, nx::format("readSomeAsync internalSize=%1").arg(recvSize));
            handler(SystemError::noError, (size_t)recvSize);
        });
}

void BufferedStreamSocket::triggerCatchRecvEvent(SystemError::ErrorCode resultCode)
{
    post(
        [this, resultCode]()
        {
            nx::utils::swapAndCall(m_catchRecvEventHandler, resultCode);
        });
}

} // namespace nx::network
