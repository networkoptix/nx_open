#pragma once

#include <memory>

#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>

#include "message_dispatcher.h"
#include "server_connection.h"

namespace nx {
namespace network {
namespace stun {

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
        const nx::network::http::StringType& stunOverHttpPath)
    {
        using namespace std::placeholders;
        using CreateStunOverHttpConnectionHandler = nx::network::http::server::handler::CreateTunnelHandler;

        httpMessageDispatcher->template registerRequestProcessor<CreateStunOverHttpConnectionHandler>(
            stunOverHttpPath,
            [this]()
            {
                return std::make_unique<CreateStunOverHttpConnectionHandler>(
                    kStunProtocolName,
                    std::bind(&StunOverHttpServer::createStunConnection, this, _1));
            });
    }

    StunConnectionPool& stunConnectionPool();
    const StunConnectionPool& stunConnectionPool() const;

private:
    StunConnectionPool m_stunConnectionPool;
    nx::network::stun::MessageDispatcher* m_dispatcher;

    void createStunConnection(std::unique_ptr<AbstractStreamSocket> connection);
};

} // namespace stun
} // namespace network
} // namespace nx
