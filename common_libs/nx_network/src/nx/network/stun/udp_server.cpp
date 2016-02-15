/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#include "udp_server.h"

#include <memory>

#include <nx/utils/log/log.h>

#include "abstract_server_connection.h"
#include "message_dispatcher.h"
#include "udp_message_response_sender.h"


namespace nx {
namespace stun {

static const std::chrono::seconds kRetryReadAfterFailureTimeout(1);

UDPServer::UDPServer(const MessageDispatcher& dispatcher)
:
    m_messagePipeline(this),
    m_boundToLocalAddress(false),
    m_dispatcher(dispatcher)
{
}

UDPServer::~UDPServer()
{
    pleaseStopSync();
}

void UDPServer::pleaseStop(std::function<void()> handler)
{
    m_messagePipeline.pleaseStop(std::move(handler));
}

bool UDPServer::bind(const SocketAddress& localAddress)
{
    m_boundToLocalAddress = true;
    return m_messagePipeline.bind(localAddress);
}

bool UDPServer::listen()
{
    if (!m_boundToLocalAddress)
    {
        if (!bind(SocketAddress(HostAddress::anyHost, 0)))
            return false;
        m_boundToLocalAddress = true;
    }
    m_messagePipeline.startReceivingMessages();
    return true;
}

SocketAddress UDPServer::address() const
{
    return m_messagePipeline.address();
}

void UDPServer::sendMessage(
    SocketAddress destinationEndpoint,
    const Message& message,
    std::function<void(SystemError::ErrorCode)> completionHandler)
{
    m_messagePipeline.sendMessage(
        std::move(destinationEndpoint),
        message,
        [completionHandler](    //TODO #ak #msvc2015 move to lambda
            SystemError::ErrorCode errorCode,
            SocketAddress resolvedTargetAddress)
        {
            if (completionHandler)
                completionHandler(errorCode);
        });
}

network::UDPSocket& UDPServer::socket()
{
    return m_messagePipeline.socket();
}

void UDPServer::messageReceived(SocketAddress sourceAddress, Message mesage)
{
    m_dispatcher.dispatchRequest(
        std::make_shared<UDPMessageResponseSender>(this, std::move(sourceAddress)),
        std::move(mesage));
}

void UDPServer::ioFailure(SystemError::ErrorCode)
{
    //TODO #ak
}

}   //stun
}   //nx
