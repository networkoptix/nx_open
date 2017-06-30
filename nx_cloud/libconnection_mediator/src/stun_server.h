#pragma once

#include <memory>
#include <vector>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/udp_server.h>

namespace nx {
namespace hpm {

namespace conf { class Settings; };

class StunServer
{
public:
    StunServer(const conf::Settings& settings);

    void listen();
    void stopAcceptingNewRequests();

    const std::vector<SocketAddress>& endpoints() const;
    nx::stun::MessageDispatcher& dispatcher();

private:
    const conf::Settings& m_settings;
    nx::stun::MessageDispatcher m_stunMessageDispatcher;
    std::unique_ptr<nx::network::server::MultiAddressServer<stun::SocketServer>> m_tcpStunServer;
    std::unique_ptr<nx::network::server::MultiAddressServer<stun::UdpServer>> m_udpStunServer;
    std::vector<SocketAddress> m_endpoints;

    bool bind();
};

} // namespace hpm
} // namespace nx
