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
    // This class emulates "server connection" on top of UDP for convenience.
    // So, connection-related logic like this one is not appropriate.
    // Adding assert to make sure noone waits forever for this object to report "connection closed".
    NX_ASSERT(false);
}

AbstractCommunicatingSocket* UDPMessageResponseSender::socket()
{
    return m_udpServer->socket().get();
}

void UDPMessageResponseSender::close()
{
    // This class emulates "server connection" on top of UDP for convenience.
    // So, connection-related logic like this one is not appropriate.
}

void UDPMessageResponseSender::setInactivityTimeout(
    boost::optional<std::chrono::milliseconds> /*value*/)
{
    // This class emulates "server connection" on top of UDP for convenience.
    // So, connection-related logic like this one is not appropriate.
}

} // namespace stun
} // namespace nx
