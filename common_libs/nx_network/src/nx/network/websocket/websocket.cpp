#include "websocket.h"
#include <nx/utils/log/log.h>

namespace nx {
namespace network {

using namespace websocket;

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
    m_isFirstFrame(true),
    m_readBuffer(nullptr)
{
    nx::Buffer tmpBuf(requestData);
    m_parser.consume(tmpBuf);
    bindToAioThread(m_baseConnection.getAioThread());
    m_baseConnection.dispatch([this](){ handleRead(); });
}

WebSocket::~WebSocket()
{
}

void WebSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    AbstractAsyncChannel::bindToAioThread(aioThread);
    m_baseConnection.bindToAioThread(aioThread);
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

void WebSocket::readyToSendData(size_t count)
{
    m_writeBuffer.clear();
    auto writeHandlerCopy = std::move(m_writeHandler);
    m_writeHandler = nullptr;
    if (writeHandlerCopy)
        writeHandlerCopy(SystemError::noError, count);
}

void WebSocket::handleRead()
{
    if (!m_readHandler)
        return;

    if (m_buffer.readySize() == 0)
        return;

    decltype(m_readHandler) handlerCopy = std::move(m_readHandler);
    m_readHandler = nullptr;
    m_baseConnection.stopReading();
    auto handoutBuffer = m_buffer.pop();

    NX_LOG(lit("[WebSocket] handleRead(): user data size: %1").arg(handoutBuffer.size()), cl_logDEBUG2);

    *m_readBuffer->append(handoutBuffer);
    handlerCopy(SystemError::noError, handoutBuffer.size());
}

void WebSocket::readSomeAsync(
    nx::Buffer* const buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    m_readHandler = std::move(handler);
    m_readBuffer = buffer;

    if (m_buffer.readySize() != 0)
        m_baseConnection.dispatch([this]() { handleRead(); });
    else
        m_baseConnection.startReadingConnection();
}

void WebSocket::sendAsync(
    const nx::Buffer& buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_sendMode == SendMode::singleMessage)
    {
        m_serializer.prepareFrame(buffer, FrameType::binary, true, &m_writeBuffer);
    }
    else
    {
        FrameType type = !m_isFirstFrame ? FrameType::continuation : FrameType::binary;

        m_serializer.prepareFrame(buffer, type, m_isLastFrame, &m_writeBuffer);
        m_isFirstFrame = m_isLastFrame;
        if (m_isLastFrame)
            m_isLastFrame = false;
    }

    m_writeHandler = std::move(handler);
    m_baseConnection.sendBufAsync(m_writeBuffer);
}

void WebSocket::cancelIOSync(nx::network::aio::EventType /*eventType*/)
{
    m_baseConnection.pleaseStopSync();
}

void WebSocket::closeConnection(SystemError::ErrorCode closeReason, ConnectionType* /*connection*/)
{
    m_readHandler(closeReason, 0);
    m_baseConnection.pleaseStopSync();
}


void WebSocket::frameStarted(FrameType /*type*/, bool /*fin*/)
{
    NX_LOG(lit("[WebSocket] frame started. type: %1, size from header: %2")
           .arg(m_parser.frameType())
           .arg(m_parser.frameSize()), cl_logDEBUG2);
}

void WebSocket::framePayload(const char* data, int len)
{
    m_buffer.append(data, len);

    if (m_receiveMode == ReceiveMode::stream)
        m_buffer.lock();
}

void WebSocket::frameEnded()
{
    NX_LOG(lit("[WebSocket] frame ended. type: %1, size from header: %2")
           .arg(m_parser.frameType())
           .arg(m_parser.frameSize()), cl_logDEBUG2);

    if (m_receiveMode != ReceiveMode::frame)
        return;

    m_buffer.lock();
}

void WebSocket::messageEnded()
{
    NX_LOG(lit("[WebSocket] message ended"), cl_logDEBUG2);

    if (m_receiveMode != ReceiveMode::message)
        return;

    m_buffer.lock();
}

void WebSocket::handleError(Error err)
{
    NX_LOG(lit("[WebSocket] Parse error %1. Closing connection").arg((int)err), cl_logDEBUG1);
    closeConnection(SystemError::invalidData, nullptr);
}

} // namespace network
} // namespace nx