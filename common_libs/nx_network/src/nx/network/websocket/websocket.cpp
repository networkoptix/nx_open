#include "websocket.h"

namespace nx {
namespace network {
namespace websocket {

WebSocket::WebSocket(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    SendMode sendMode,
    ReceiveMode receiveMode,
    Role role)
    :
    m_socket(std::move(streamSocket)),
    m_parser(role, this),
    m_serializer(role == Role::client),
    m_sendMode(sendMode),
    m_receiveMode(receiveMode),
    m_isLastFrame(false),
    m_isFirstFrame(true),
    m_pingTimer(new nx::network::aio::Timer),
    m_terminating(false),
    m_pingTimeout(std::chrono::seconds(100))
{
    AbstractAsyncChannel::bindToAioThread(m_socket->getAioThread());
    m_pingTimer->bindToAioThread(m_socket->getAioThread());
    setPingTimeout(m_pingTimeout);
    m_readBuffer.reserve(4096);
}

void WebSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    AbstractAsyncChannel::bindToAioThread(aioThread);
    m_socket->bindToAioThread(aioThread);
    m_pingTimer->cancelSync();
    m_pingTimer->bindToAioThread(aioThread);
    setPingTimeout(m_pingTimeout);
}

void WebSocket::stopWhileInAioThread()
{
    m_socket.reset();
    m_pingTimer.reset();
}

void WebSocket::setIsLastFrame()
{
    dispatch([this]() { m_isLastFrame = true; });
}

void WebSocket::handleSocketRead(SystemError::ErrorCode ecode, size_t bytesRead)
{
    m_parser.consume(m_readBuffer.data(), bytesRead);
    m_readBuffer.resize(0);
    processReadData(ecode);
}

void WebSocket::processReadData(SystemError::ErrorCode ecode)
{
    if (ecode != SystemError::noError)
    {
        auto readData = m_readQueue.pop();
        readData.handler(ecode, 0);
        return;
    }

    if (m_userDataBuffer.readySize() == 0)
    {
        m_socket->readSomeAsync(
            &m_readBuffer,
            [this](SystemError::ErrorCode ecode, size_t bytesRead)
            {
                handleSocketRead(ecode, bytesRead);
            });
        return;
    }

    auto readData = m_readQueue.pop();
    bool queueEmpty = m_readQueue.empty();
    auto handoutBuffer = m_userDataBuffer.popFront();

    NX_LOG(lit("[WebSocket] handleRead(): user data size: %1").arg(handoutBuffer.size()), cl_logDEBUG2);

    readData.buffer->append(handoutBuffer);
    readData.handler(ecode, handoutBuffer.size());
    if (queueEmpty)
        return;
    readWithoutAddingToQueueAsync();
}

void WebSocket::readWithoutAddingToQueueAsync()
{
    post([this]() { readWithoutAddingToQueue(); });
}

void WebSocket::readWithoutAddingToQueue()
{
    if (m_userDataBuffer.readySize() != 0)
    {
        processReadData(SystemError::noError);
    }
    else
    {
        m_socket->readSomeAsync(
            &m_readBuffer,
            [this](SystemError::ErrorCode ecode, size_t bytesRead)
            {
                handleSocketRead(ecode, bytesRead);
            });
    }
}

void WebSocket::readSomeAsync(nx::Buffer* const buffer, HandlerType handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            if (m_terminating)
            {
                NX_LOG("[WebSocket] readSomeAsync called after connection has been terminated. Ignoring.", cl_logDEBUG1);
                handler(SystemError::connectionAbort, 0);
                return;
            }

            setPingTimeout(m_pingTimeout);

            bool queueEmpty = m_readQueue.empty();
            nx::Buffer* tmp = buffer;
            m_readQueue.emplace(std::move(handler), std::move(tmp));
            if (queueEmpty)
                readWithoutAddingToQueue();
        });
}

void WebSocket::sendAsync(const nx::Buffer& buffer, HandlerType handler)
{
    post(
        [this, &buffer, handler = std::move(handler)]() mutable
        {
            if (m_terminating)
            {
                NX_LOG("[WebSocket] readSomeAsync called after connection has been terminated. Ignoring.", cl_logDEBUG1);
                handler(SystemError::connectionAbort, 0);
                return;
            }

            setPingTimeout(m_pingTimeout);

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

void WebSocket::handlePingTimer()
{
    sendControlRequest(FrameType::ping);
    m_pingTimer->start(m_pingTimeout, [this]() { handlePingTimer(); });
}

void WebSocket::setPingTimeout(std::chrono::milliseconds timeout)
{
    dispatch(
        [this, timeout]()
        {
            m_pingTimer->cancelSync();
            m_pingTimeout = timeout;
            m_pingTimer->start(m_pingTimeout, [this]() { handlePingTimer(); });
        });
}

void WebSocket::sendCloseAsync()
{
    dispatch(
        [this]()
        {
            sendControlRequest(FrameType::close);
            m_terminating = true;
            emit connectionClosed();
        });
}

void WebSocket::handleSocketWrite(SystemError::ErrorCode ecode, size_t /*bytesSent*/)
{
    auto writeData = m_writeQueue.pop();
    bool queueEmpty = m_writeQueue.empty();
    if (writeData.handler)
        writeData.handler(ecode, writeData.buffer.writeSize);
    if (!queueEmpty)
        m_socket->sendAsync(
            m_writeQueue.first().buffer.buffer,
            [this](SystemError::ErrorCode ecode, size_t bytesSent)
            {
                handleSocketWrite(ecode, bytesSent);
            });
}

void WebSocket::sendPreparedMessage(nx::Buffer* buffer, int writeSize, HandlerType handler)
{
    WriteData writeData(std::move(*buffer), writeSize);
    bool queueEmpty = m_writeQueue.empty();
    m_writeQueue.emplace(std::move(handler), std::move(writeData));
    if (queueEmpty)
        m_socket->sendAsync(
            m_writeQueue.first().buffer.buffer,
            [this](SystemError::ErrorCode ecode, size_t bytesSent)
            {
                handleSocketWrite(ecode, bytesSent);
            });
}

void WebSocket::cancelIOSync(nx::network::aio::EventType eventType)
{
    m_socket->cancelIOSync(eventType);
    m_pingTimer->cancelPostedCallsSync();
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
    {
        m_controlBuffer.append(data, len);
        return;
    }

    m_userDataBuffer.append(data, len);

    if (m_receiveMode == ReceiveMode::stream)
        m_userDataBuffer.lock();
}

void WebSocket::frameEnded()
{
    NX_LOG(lit("[WebSocket] frame ended. type: %1, size from header: %2")
        .arg(m_parser.frameType())
        .arg(m_parser.frameSize()), cl_logDEBUG2);

    if (m_parser.frameType() == FrameType::ping)
    {
        emit pingReceived();
        sendControlResponse(FrameType::ping, FrameType::pong);
        return;
    }

    if (m_parser.frameType() == FrameType::pong)
    {
        NX_ASSERT(m_controlBuffer.isEmpty());
        NX_LOG("[WebSocket] Received pong", cl_logDEBUG2);
        emit pongReceived();
        return;
    }

    if (m_parser.frameType() == FrameType::close)
    {
        if (!m_terminating)
        {
            NX_LOG("[WebSocket] Received close request, responding and terminating", cl_logDEBUG1);
            sendControlResponse(FrameType::close, FrameType::close);
            m_terminating = true;
            emit connectionClosed();
            return;
        }
        else
        {
            NX_LOG("[WebSocket] Received close response, terminating", cl_logDEBUG1);
            NX_ASSERT(m_controlBuffer.isEmpty());
        }

        return;
    }

    if (!isDataFrame())
    {
        NX_LOG(lit("[WebSocket] Unknown frame %1").arg(m_parser.frameType()), cl_logWARNING);
        m_controlBuffer.resize(0);
        return;
    }

    if (m_receiveMode != ReceiveMode::frame)
        return;

    m_userDataBuffer.lock();
}

void WebSocket::sendControlResponse(FrameType requestType, FrameType responseType)
{
    nx::Buffer responseFrame;
    m_serializer.prepareMessage(m_controlBuffer, responseType, &responseFrame);
    auto writeSize = m_controlBuffer.size();
    m_controlBuffer.resize(0);
    sendPreparedMessage(
        &responseFrame,
        writeSize,
        [this, requestType](SystemError::ErrorCode, size_t)
        {
            NX_LOG(lit("[WebSocket] control response for %1 successfully sent")
                .arg(frameTypeString(requestType)), cl_logDEBUG2);
        });
}

void WebSocket::sendControlRequest(FrameType type)
{
    nx::Buffer requestFrame;
    m_serializer.prepareMessage("", type, &requestFrame);
    sendPreparedMessage(
        &requestFrame,
        0,
        [this, type](SystemError::ErrorCode, size_t)
        {
            NX_LOG(lit("[WebSocket] control request %1 successfully sent")
                .arg(frameTypeString(type)), cl_logDEBUG2);
        });
}

void WebSocket::messageEnded()
{
    NX_LOG(lit("[WebSocket] message ended"), cl_logDEBUG2);

    if (!isDataFrame())
        return;

    if (m_receiveMode != ReceiveMode::message)
        return;

    m_userDataBuffer.lock();
}

void WebSocket::handleError(Error err)
{
    NX_LOG(lit("[WebSocket] Parse error %1. Closing connection").arg((int)err), cl_logDEBUG1);
    sendCloseAsync();
    emit connectionClosed();
}

QString frameTypeString(FrameType type)
{
    switch (type)
    {
    case FrameType::continuation: return lit("continuation");
    case FrameType::text: return lit("text");
    case FrameType::binary: return lit("binary");
    case FrameType::close: return lit("close");
    case FrameType::ping: return lit("ping");
    case FrameType::pong: return lit("pong");
    }

    return lit("");
}

} // namespace websocket
} // namespace network
} // namespace nx
