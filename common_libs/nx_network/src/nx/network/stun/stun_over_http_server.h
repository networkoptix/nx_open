#pragma once

#include <memory>

#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>

#include "message_dispatcher.h"
#include "server_connection.h"

namespace nx {
namespace stun {

class NX_NETWORK_API StunOverHttpServer
{
public:
    using StunConnectionPool = 
        nx::network::server::StreamServerConnectionHolder<nx::stun::ServerConnection>;

    static const char* const kStunProtocolName;

    StunOverHttpServer(nx::stun::MessageDispatcher* stunMessageDispatcher);

    template<typename HttpMessageDispatcherType>
    void setupHttpTunneling(
        HttpMessageDispatcherType* httpMessageDispatcher,
        const nx_http::StringType& stunOverHttpPath)
    {
        using namespace std::placeholders;
        using CreateStunOverHttpConnectionHandler = nx_http::server::handler::CreateTunnelHandler;

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
    nx::stun::MessageDispatcher* m_dispatcher;

    void createStunConnection(std::unique_ptr<AbstractStreamSocket> connection);
};

} // namespace stun
} // namespace nx
