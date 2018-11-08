#include "udp_server.h"

#include <memory>

#include <nx/utils/log/log.h>

#include "abstract_server_connection.h"
#include "message_dispatcher.h"
#include "udp_message_response_sender.h"

namespace nx {
namespace network {
namespace stun {

UdpServer::UdpServer(const MessageDispatcher* dispatcher):
    m_messagePipeline(this),
    m_boundToLocalAddress(false),
    m_dispatcher(dispatcher)
{
    bindToAioThread(getAioThread());
}

UdpServer::~UdpServer()
{
    pleaseStopSync();
}

void UdpServer::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    network::aio::BasicPollable::bindToAioThread(aioThread);

    m_messagePipeline.bindToAioThread(aioThread);
}

bool UdpServer::bind(const SocketAddress& localAddress)
{
    m_boundToLocalAddress = true;
    return m_messagePipeline.bind(localAddress);
}

bool UdpServer::listen(int /*backlogSize*/)
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

void UdpServer::stopReceivingMessagesSync()
{
    m_messagePipeline.stopReceivingMessagesSync();
}

SocketAddress UdpServer::address() const
{
    return m_messagePipeline.address();
}

void UdpServer::sendMessage(
    SocketAddress destinationEndpoint,
    const Message& message,
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> completionHandler)
{
    m_messagePipeline.sendMessage(
        std::move(destinationEndpoint), message,
        [completionHandler = std::move(completionHandler)](
            SystemError::ErrorCode errorCode, SocketAddress) mutable
        {
            if (completionHandler)
                completionHandler(errorCode);
        });
}

const std::unique_ptr<network::UDPSocket>& UdpServer::socket()
{
    return m_messagePipeline.socket();
}

nx::network::server::Statistics UdpServer::statistics() const
{
    // TODO: #ak
    return nx::network::server::Statistics();
}

void UdpServer::stopWhileInAioThread()
{
    m_messagePipeline.pleaseStopSync();
}

void UdpServer::messageReceived(SocketAddress sourceAddress, Message mesage)
{
    m_dispatcher->dispatchRequest(
        std::make_shared<UDPMessageResponseSender>(this, std::move(sourceAddress)),
        std::move(mesage));
}

void UdpServer::ioFailure(SystemError::ErrorCode)
{
    //TODO #ak
}

} // namespace stun
} // namespace network
} // namespace nx
