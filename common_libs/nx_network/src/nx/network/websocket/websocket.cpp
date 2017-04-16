#include <nx/network/websocket/websocket.h>
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
    m_sendMode(SendMode::singleMessage),
    m_receiveMode(ReceiveMode::message),
    m_isLastFrame(false),
    m_payloadType(PayloadType::binary),
    m_readBuffer(nullptr)
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
    m_writeHandler(SystemError::noError, count);
}

void Websocket::readSomeAsync(
    nx::Buffer* const buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    m_baseConnection.startReadingConnection();
    m_readHandler = handler;
    m_readBuffer = buffer;
}

void Websocket::sendAsync(
    const nx::Buffer& buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    m_baseConnection.sendBufAsync(buffer);
    m_writeHandler = handler;
}

void Websocket::cancelIOSync(nx::network::aio::EventType eventType)
{

}

void Websocket::closeConnection(SystemError::ErrorCode closeReason, ConnectionType* connection)
{

}


void Websocket::frameStarted(FrameType type, bool fin)
{

}

void Websocket::framePayload(const char* data, int len)
{

}

void Websocket::frameEnded()
{

}

void Websocket::messageEnded()
{

}

void Websocket::handleError(Error err)
{

}

}
}
}