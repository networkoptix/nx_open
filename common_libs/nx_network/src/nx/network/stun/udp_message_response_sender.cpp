/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#include "udp_message_response_sender.h"

#include "udp_server.h"


namespace nx {
namespace stun {

UDPMessageResponseSender::UDPMessageResponseSender(
    UDPServer* udpServer,
    SocketAddress sourceAddress)
:
    m_udpServer(udpServer),
    m_sourceAddress(std::move(sourceAddress))
{
}

UDPMessageResponseSender::~UDPMessageResponseSender()
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

void UDPMessageResponseSender::addOnConnectionCloseHandler(std::function<void()> /*handler*/)
{
    //TODO #ak
    NX_ASSERT(false);
}

AbstractCommunicatingSocket* UDPMessageResponseSender::socket()
{
    return m_udpServer->socket().get();
}

}   //stun
}   //nx
