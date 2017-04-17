#include "websocket.h"
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace websocket {

Websocket::Websocket(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    const nx::Buffer& requestData,
    Role role)
    :
    m_baseConnection(this, std::move(streamSocket), this),
    m_parser(role, this),
    m_serializer(role == Role::client),
    m_sendMode(SendMode::singleMessage),
    m_receiveMode(ReceiveMode::message),
    m_isLastFrame(false),
    m_isFirstFrame(true),
    m_payloadType(PayloadType::binary),
    m_readBuffer(nullptr),
    m_requestData(requestData)
{}

void Websocket::setSendMode(SendMode mode)
{
    m_sendMode = mode;
}

Websocket::SendMode Websocket::sendMode() const
{
    return m_sendMode;
}

void Websocket::setIsLastFrame()
{
    m_isLastFrame = true;
}

void Websocket::setReceiveMode(ReceiveMode mode)
{
    m_receiveMode = mode;
}

Websocket::ReceiveMode Websocket::receiveMode() const
{
    return m_receiveMode;
}

void Websocket::setPayloadType(PayloadType type)
{
    m_payloadType = type;
}

PayloadType Websocket::prevFramePayloadType() const
{
    return m_receivedPayloadType;
}

void Websocket::bytesReceived(nx::Buffer& buffer)
{
    m_parser.consume(buffer.data(), buffer.size());
}

void Websocket::readyToSendData(size_t count)
{
    m_writeBuffer.clear();
    m_writeHandler(SystemError::noError, count);
}

void Websocket::readSomeAsync(
    nx::Buffer* const buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    m_readHandler = std::move(handler);
    m_readBuffer = buffer;

    if (!m_requestData.isEmpty())
    {
        m_baseConnection.dispatch(
            [this]()
            {
                m_parser.consume(m_requestData.data(), m_requestData.size());
                m_requestData.clear();
            });
    }
    else
    {
        m_baseConnection.startReadingConnection();
    }
}

void Websocket::sendAsync(
    const nx::Buffer& buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_sendMode == SendMode::singleMessage)
    {
        FrameType type = m_payloadType == PayloadType::binary ? FrameType::binary : FrameType::text;
        m_serializer.prepareFrame(buffer, type, true, &m_writeBuffer);
    }
    else
    {
        FrameType type = !m_isFirstFrame 
            ? FrameType::continuation 
            : m_payloadType == PayloadType::binary 
                ? FrameType::binary 
                : FrameType::text;

        m_serializer.prepareFrame(buffer, type, m_isLastFrame, &m_writeBuffer);
        m_isFirstFrame = m_isLastFrame;
        if (m_isLastFrame)
            m_isLastFrame = false;
    }

    m_writeHandler = std::move(handler);
    m_baseConnection.sendBufAsync(buffer);
}

void Websocket::cancelIOSync(nx::network::aio::EventType /*eventType*/)
{
    m_baseConnection.pleaseStopSync();
}

void Websocket::closeConnection(SystemError::ErrorCode closeReason, ConnectionType* /*connection*/)
{
    m_readHandler(closeReason, 0);
    m_baseConnection.pleaseStopSync();
}


void Websocket::frameStarted(FrameType /*type*/, bool /*fin*/)
{
    NX_LOG(lit("[Websocket] frame started. type: %1, size from header: %2")
           .arg(m_parser.frameType())
           .arg(m_parser.frameSize()), cl_logDEBUG2);
}

void Websocket::framePayload(const char* data, int len)
{
    if (m_receiveMode == ReceiveMode::stream)
    {
        m_readBuffer->append(data, len);
        m_readHandler(SystemError::noError, (size_t)len);
        return;
    }

    m_buffer.append(data, len);
}

void Websocket::frameEnded()
{
    NX_LOG(lit("[Websocket] frame ended. type: %1, size from header: %2, actual size: %3")
           .arg(m_parser.frameType())
           .arg(m_parser.frameSize())
           .arg(m_buffer.size()), cl_logDEBUG2);

    if (m_receiveMode != ReceiveMode::frame)
        return;

    m_readBuffer->append(m_buffer);
    m_readHandler(SystemError::noError, m_buffer.size());
    m_buffer.clear();
}

void Websocket::messageEnded()
{
    NX_LOG(lit("[Websocket] message ended. type: %1, total message size: %2")
           .arg(m_parser.frameType())
           .arg(m_buffer.size()), cl_logDEBUG2);

    if (m_receiveMode != ReceiveMode::message)
        return;

    m_readBuffer->append(m_buffer);
    m_readHandler(SystemError::noError, m_buffer.size());
    m_buffer.clear();
}

void Websocket::handleError(Error err)
{
    NX_LOG(lit("[Websocket] Parse error %1. Closing connection").arg((int)err), cl_logDEBUG1);
    closeConnection(SystemError::invalidData, nullptr);
}

} // namespace websocket
} // namespace network
} // namespace nx