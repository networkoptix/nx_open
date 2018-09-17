#pragma once

#include <memory>

#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>
#include <nx/network/http/tunneling/server.h>

#include "message_dispatcher.h"
#include "server_connection.h"

namespace nx {
namespace network {
namespace stun {

/**
 * NOTE: Uses nx::network::http::tunneling::Server inside.
 */
class NX_NETWORK_API StunOverHttpServer
{
public:
    using StunConnectionPool =
        nx::network::server::StreamServerConnectionHolder<nx::network::stun::ServerConnection>;

    static const char* const kStunProtocolName;

    StunOverHttpServer(nx::network::stun::MessageDispatcher* stunMessageDispatcher);

    template<typename HttpMessageDispatcherType>
    void setupHttpTunneling(
        HttpMessageDispatcherType* httpMessageDispatcher,
        const std::string& stunOverHttpPath)
    {
        using namespace std::placeholders;
        using CreateStunOverHttpConnectionHandler = nx::network::http::server::handler::CreateTunnelHandler;

        // TODO: #ak Remove it after end of 3.2 support.
        httpMessageDispatcher->template registerRequestProcessor<CreateStunOverHttpConnectionHandler>(
            stunOverHttpPath.c_str(),
            [this]()
            {
                return std::make_unique<CreateStunOverHttpConnectionHandler>(
                    kStunProtocolName,
                    std::bind(&StunOverHttpServer::createStunConnection, this, _1));
            });

        m_httpTunnelingServer.registerRequestHandlers(
            stunOverHttpPath,
            httpMessageDispatcher);
    }

    StunConnectionPool& stunConnectionPool();
    const StunConnectionPool& stunConnectionPool() const;

private:
    StunConnectionPool m_stunConnectionPool;
    nx::network::stun::MessageDispatcher* m_dispatcher;
    nx::network::http::tunneling::Server<> m_httpTunnelingServer;

    void createStunConnection(std::unique_ptr<AbstractStreamSocket> connection);
};

} // namespace stun
} // namespace network
} // namespace nx
