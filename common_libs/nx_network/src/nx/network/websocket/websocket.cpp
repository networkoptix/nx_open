#include "websocket.h"
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace websocket {

WebSocket::WebSocket(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    const nx::Buffer& requestData,
    SendMode sendMode,
    ReceiveMode receiveMode,
    Role role)
    :
    m_baseConnection(this, std::move(streamSocket), this),
    m_parser(role, this),
    m_serializer(role == Role::client),
    m_sendMode(sendMode),
    m_receiveMode(receiveMode),
    m_isLastFrame(false),
    m_isFirstFrame(true)
{
    nx::Buffer tmpBuf(requestData);
    m_parser.consume(tmpBuf);
    AbstractAsyncChannel::bindToAioThread(m_baseConnection.getAioThread());
}

void WebSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    dispatch(
        [this, aioThread]()
        {
            AbstractAsyncChannel::bindToAioThread(aioThread);
            m_baseConnection.bindToAioThread(aioThread);
        });
}

void WebSocket::stopWhileInAioThread()
{
    m_baseConnection.pleaseStopSync();
}

void WebSocket::setIsLastFrame()
{
    m_isLastFrame = true;
}

void WebSocket::bytesReceived(nx::Buffer& buffer)
{
    m_parser.consume(buffer.data(), buffer.size());
    handleRead();
}

void WebSocket::readyToSendData()
{
    auto writeData = m_writeQueue.pop();
    bool queueEmpty = m_writeQueue.empty();
    if (writeData.handler)
        writeData.handler(SystemError::noError, writeData.buffer.writeSize);
    if (!queueEmpty)
        m_baseConnection.sendBufAsync(m_writeQueue.first().buffer.buffer);
}

void WebSocket::handleRead()
{
    if (m_buffer.readySize() == 0)
    {
        m_baseConnection.setReceivingStarted();
        return;
    }

    auto readData = m_readQueue.pop();
    bool queueEmpty = m_readQueue.empty();
    auto handoutBuffer = m_buffer.pop();

    NX_LOG(lit("[WebSocket] handleRead(): user data size: %1").arg(handoutBuffer.size()), cl_logDEBUG2);

    readData.buffer->append(handoutBuffer);
    readData.handler(SystemError::noError, handoutBuffer.size());
    if (queueEmpty)
        m_baseConnection.stopReading();
    else
        readWithoutAddingToQueue();
}

void WebSocket::readWithoutAddingToQueue()
{
    post(
        [this]()
        {
            if (m_buffer.readySize() != 0)
                handleRead();
            else
                m_baseConnection.startReadingConnection();
        });
}

void WebSocket::readSomeAsync(nx::Buffer* const buffer, HandlerType handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            bool queueEmpty = m_readQueue.empty();
            nx::Buffer* tmp = buffer;
            m_readQueue.emplace(std::move(handler), std::move(tmp));

            if (m_buffer.readySize() != 0)
                handleRead();
            else if (queueEmpty)
                m_baseConnection.startReadingConnection();
        });
}

void WebSocket::sendAsync(const nx::Buffer& buffer, HandlerType handler)
{
    post(
        [this, &buffer, handler = std::move(handler)]()
        {
            nx::Buffer writeBuffer;
            if (m_sendMode == SendMode::singleMessage)
            {
                m_serializer.prepareMessage(buffer, FrameType::binary, &writeBuffer);
            }
            else
            {
                FrameType type = !m_isFirstFrame ? FrameType::continuation : FrameType::binary;

                m_serializer.prepareFrame(buffer, type, m_isLastFrame, &writeBuffer);
                m_isFirstFrame = m_isLastFrame;
                if (m_isLastFrame)
                    m_isLastFrame = false;
            }

            sendPreparedMessage(&writeBuffer, buffer.size(), std::move(handler));
        });
}

void WebSocket::sendPreparedMessage(nx::Buffer* buffer, int writeSize, HandlerType handler)
{
    WriteData writeData(std::move(*buffer), writeSize);
    bool queueEmpty = m_writeQueue.empty();
    m_writeQueue.emplace(std::move(handler), std::move(writeData));
    if (queueEmpty)
        m_baseConnection.sendBufAsync(m_writeQueue.first().buffer.buffer);
}

void WebSocket::cancelIOSync(nx::network::aio::EventType /*eventType*/)
{
    dispatch(
        [this]()
        {
            m_baseConnection.pleaseStopSync();
        });
}

void WebSocket::closeConnection(SystemError::ErrorCode closeReason, ConnectionType* /*connection*/)
{
    m_baseConnection.pleaseStopSync();
}

bool WebSocket::isDataFrame() const
{
    return m_parser.frameType() == FrameType::binary
        || m_parser.frameType() == FrameType::text
        || m_parser.frameType() == FrameType::continuation;
}

void WebSocket::frameStarted(FrameType /*type*/, bool /*fin*/)
{
    NX_LOG(lit("[WebSocket] frame started. type: %1, size from header: %2")
        .arg(m_parser.frameType())
        .arg(m_parser.frameSize()), cl_logDEBUG2);
}

void WebSocket::framePayload(const char* data, int len)
{
    if (!isDataFrame())
        return;

    m_buffer.append(data, len);

    if (m_receiveMode == ReceiveMode::stream)
        m_buffer.lock();
}

void WebSocket::frameEnded()
{
    NX_LOG(lit("[WebSocket] frame ended. type: %1, size from header: %2")
        .arg(m_parser.frameType())
        .arg(m_parser.frameSize()), cl_logDEBUG2);

    if (m_parser.frameType() == FrameType::ping)
    {
        return;
    }

    if (!isDataFrame())
        return;

    if (m_receiveMode != ReceiveMode::frame)
        return;

    m_buffer.lock();
}

void WebSocket::messageEnded()
{
    NX_LOG(lit("[WebSocket] message ended"), cl_logDEBUG2);

    if (!isDataFrame())
        return;

    if (m_receiveMode != ReceiveMode::message)
        return;

    m_buffer.lock();
}

void WebSocket::handleError(Error err)
{
    NX_LOG(lit("[WebSocket] Parse error %1. Closing connection").arg((int)err), cl_logDEBUG1);
    closeConnection(SystemError::invalidData, nullptr);
}

} // namespace websocket
} // namespace network
} // namespace nx