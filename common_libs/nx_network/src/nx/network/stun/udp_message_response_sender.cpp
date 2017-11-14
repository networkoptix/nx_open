#include "udp_message_response_sender.h"

#include "udp_server.h"

namespace nx {
namespace stun {

UDPMessageResponseSender::UDPMessageResponseSender(
    UdpServer* udpServer,
    SocketAddress sourceAddress)
:
    m_udpServer(udpServer),
    m_sourceAddress(std::move(sourceAddress))
{
}

void UDPMessageResponseSender::sendMessage(
    nx::stun::Message message,
    std::function<void(SystemError::ErrorCode)> handler)
{
    m_udpServer->sendMessage(
        m_sourceAddress,
        std::move(message),
        std::move(handler));
}

nx::network::TransportProtocol UDPMessageResponseSender::transportProtocol() const
{
    return nx::network::TransportProtocol::udp;
}

SocketAddress UDPMessageResponseSender::getSourceAddress() const
{
    return m_sourceAddress;
}

void UDPMessageResponseSender::addOnConnectionCloseHandler(
    nx::utils::MoveOnlyFunc<void()> /*handler*/)
{
    //TODO #ak
    NX_ASSERT(false);
}

AbstractCommunicatingSocket* UDPMessageResponseSender::socket()
{
    return m_udpServer->socket().get();
}

void UDPMessageResponseSender::close()
{
    // Doing nothing, since it is UDP.
}

} // namespace stun
} // namespace nx
