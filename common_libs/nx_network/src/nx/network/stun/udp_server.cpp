/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#include "udp_server.h"

#include <memory>

#include <nx/utils/log/log.h>

#include "message_dispatcher.h"
#include "udp_message_response_sender.h"


namespace nx {
namespace stun {

static const std::chrono::seconds kRetryReadAfterFailureTimeout(1);

UDPServer::UDPServer(
    const MessageDispatcher& dispatcher,
    SocketFactory::NatTraversalType natTraversalType)
:
    m_dispatcher(dispatcher),
    m_socket(SocketFactory::createDatagramSocket(natTraversalType))
{
}

UDPServer::~UDPServer()
{
    m_socket->pleaseStopSync();
}

bool UDPServer::bind(const SocketAddress& localAddress)
{
    return m_socket->bind(localAddress);
}

bool UDPServer::listen()
{
    using namespace std::placeholders;
    m_readBuffer.resize(0);
    m_readBuffer.reserve(nx::network::kMaxUDPDatagramSize);
    m_socket->recvFromAsync(
        &m_readBuffer, std::bind(&UDPServer::onBytesRead, this, _1, _2, _3));
    return true;
}

SocketAddress UDPServer::address() const
{
    return m_socket->getLocalAddress();
}

void UDPServer::sendMessageAsync(
    SocketAddress destinationEndpoint,
    Message message,
    std::function<void(SystemError::ErrorCode)> completionHandler)
{
    m_socket->dispatch(std::bind(
        &UDPServer::sendMessageInternal,
        this,
        std::move(destinationEndpoint),
        std::move(message),
        std::move(completionHandler)));
}

void UDPServer::onBytesRead(
    SystemError::ErrorCode errorCode,
    SocketAddress sourceAddress,
    size_t bytesRead)
{
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("Error reading from socket. %1").
            arg(SystemError::toString(errorCode)), cl_logDEBUG1);
        m_socket->registerTimer(
            kRetryReadAfterFailureTimeout,
            [this](){ listen(); });
        return;
    }

    //reading and parsing message
    size_t bytesParsed = 0;
    Message msg;
    m_messageParser.setMessage(&msg);
    if (m_messageParser.parse(m_readBuffer, &bytesParsed) == nx_api::ParserState::done)
    {
        m_dispatcher.dispatchRequest(
            std::make_shared<UDPMessageResponseSender>(this, std::move(sourceAddress)),
            std::move(msg));
    }
    else
    {
        NX_LOGX(lm("Failed to parse UDP datagram of size %1").
            arg((unsigned int)bytesRead), cl_logDEBUG1);
    }

    m_readBuffer.resize(0);
    m_readBuffer.reserve(nx::network::kMaxUDPDatagramSize);
    using namespace std::placeholders;
    m_socket->recvFromAsync(
        &m_readBuffer, std::bind(&UDPServer::onBytesRead, this, _1, _2, _3));
}

void UDPServer::sendMessageInternal(
    SocketAddress destinationEndpoint,
    Message message,
    std::function<void(SystemError::ErrorCode)> completionHandler)
{
    OutgoingMessageContext msgCtx = {
        destinationEndpoint,
        std::move(message),
        std::move(completionHandler) };

    m_sendQueue.emplace_back(std::move(msgCtx));
    if (m_sendQueue.size() == 1)
        sendOutNextMessage();
}

void UDPServer::sendOutNextMessage()
{
    OutgoingMessageContext& msgCtx = m_sendQueue.front();

    m_writeBuffer.resize(0);
    m_writeBuffer.reserve(1);
    m_messageSerializer.setMessage(&msgCtx.message);
    size_t bytesWritten = 0;
    assert(m_messageSerializer.serialize(&m_writeBuffer, &bytesWritten) ==
        nx_api::SerializerState::done);
    using namespace std::placeholders;
    m_socket->sendToAsync(
        m_writeBuffer,
        msgCtx.destinationEndpoint,
        std::bind(&UDPServer::messageSent, this, _1, _2));
}

void UDPServer::messageSent(SystemError::ErrorCode errorCode, size_t /*bytesSent*/)
{
    assert(!m_sendQueue.empty());
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("Failed to send message destinationEndpoint %1. %2").
            arg(m_sendQueue.front().destinationEndpoint.toString()).arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);
    }

    if (m_sendQueue.front().completionHandler)
        m_sendQueue.front().completionHandler(errorCode);
    m_sendQueue.pop_front();

    if (!m_sendQueue.empty())
        sendOutNextMessage();
}

}   //stun
}   //nx
