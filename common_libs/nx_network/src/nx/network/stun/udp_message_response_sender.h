#pragma once

#include <functional>

#include "abstract_server_connection.h"
#include "nx/network/socket_common.h"

namespace nx {
namespace network {
namespace stun {

class UdpServer;

/**
 * Provides ability to send response to a request message received via UDP.
 */
class UDPMessageResponseSender:
    public nx::network::stun::AbstractServerConnection
{
public:
    UDPMessageResponseSender(
        UdpServer* udpServer,
        SocketAddress sourceAddress);
    virtual ~UDPMessageResponseSender() = default;

    virtual void sendMessage(
        nx::network::stun::Message message,
        std::function<void(SystemError::ErrorCode)> handler) override;
    virtual nx::network::TransportProtocol transportProtocol() const override;
    virtual SocketAddress getSourceAddress() const override;
    virtual void addOnConnectionCloseHandler(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual AbstractCommunicatingSocket* socket() override;
    virtual void close() override;
    virtual void setInactivityTimeout(std::optional<std::chrono::milliseconds> value) override;

private:
    UdpServer* m_udpServer;
    SocketAddress m_sourceAddress;
};

} // namespace stun
} // namespace network
} // namespace nx
