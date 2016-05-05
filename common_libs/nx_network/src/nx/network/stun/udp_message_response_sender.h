/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#pragma once

#include <functional>

#include "abstract_server_connection.h"
#include "nx/network/socket_common.h"


namespace nx {
namespace stun {

class UDPServer;

/** Provides ability to send response to a request message received via UDP
 */
class UDPMessageResponseSender
:
    public nx::stun::AbstractServerConnection
{
public:
    UDPMessageResponseSender(
        UDPServer* udpServer,
        SocketAddress sourceAddress);
    virtual ~UDPMessageResponseSender();

    virtual void sendMessage(
        nx::stun::Message message,
        std::function<void(SystemError::ErrorCode)> handler) override;
    virtual nx::network::TransportProtocol transportProtocol() const override;
    virtual SocketAddress getSourceAddress() const override;
    virtual void addOnConnectionCloseHandler(std::function<void()> handler) override;
    virtual AbstractCommunicatingSocket* socket() override;

private:
    UDPServer* m_udpServer;
    SocketAddress m_sourceAddress;
};

}   //stun
}   //nx
