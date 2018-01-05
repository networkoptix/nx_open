#pragma once

#include <memory>
#include <vector>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/stun_over_http_server.h>
#include <nx/network/stun/udp_server.h>

namespace nx {
namespace hpm {

namespace conf { class Settings; };
namespace http { class Server; };

class StunServer
{
public:
    StunServer(
        const conf::Settings& settings,
        http::Server* httpServer);

    void listen();
    void stopAcceptingNewRequests();

    const std::vector<network::SocketAddress>& endpoints() const;
    nx::network::stun::MessageDispatcher& dispatcher();

private:
    const conf::Settings& m_settings;
    network::stun::MessageDispatcher m_stunMessageDispatcher;
    network::stun::StunOverHttpServer m_stunOverHttpServer;
    std::unique_ptr<network::server::MultiAddressServer<network::stun::SocketServer>> m_tcpStunServer;
    std::unique_ptr<network::server::MultiAddressServer<network::stun::UdpServer>> m_udpStunServer;
    std::vector<network::SocketAddress> m_endpoints;

    void initializeHttpTunnelling(http::Server* httpServer);
    bool bind();
};

} // namespace hpm
} // namespace nx
