// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>
#include <nx/network/http/tunneling/server.h>

#include "server_connection.h"

namespace nx::network::stun {

namespace detail { class TunnelAuthorizer; }

/**
 * NOTE: Uses nx::network::http::tunneling::Server inside.
 */
class NX_NETWORK_API StunOverHttpServer
{
public:
    using StunConnectionPool =
        nx::network::server::StreamServerConnectionHolder<nx::network::stun::ServerConnection>;

    static constexpr char kStunProtocolName[] = "STUN/rfc5389";

    StunOverHttpServer(nx::network::stun::AbstractMessageHandler* messageHandler);
    ~StunOverHttpServer();

    template<typename HttpMessageDispatcherType>
    void setupHttpTunneling(
        HttpMessageDispatcherType* httpMessageDispatcher,
        const std::string& stunOverHttpPath)
    {
        using CreateStunOverHttpConnectionHandler =
            nx::network::http::server::handler::CreateTunnelHandler;

        // TODO: #akolesnikov Remove it after the end of 3.2 support.
        httpMessageDispatcher->registerRequestProcessor(
            stunOverHttpPath,
            [this]()
            {
                return std::make_unique<CreateStunOverHttpConnectionHandler>(
                    kStunProtocolName,
                    [this](auto&&... args) {
                        this->createStunConnection(std::forward<decltype(args)>(args)...);
                    });
            });

        m_httpTunnelingServer.registerRequestHandlers(
            stunOverHttpPath,
            httpMessageDispatcher);
    }

    StunConnectionPool& stunConnectionPool();
    const StunConnectionPool& stunConnectionPool() const;

    void setInactivityTimeout(std::optional<std::chrono::milliseconds> timeout);

private:
    StunConnectionPool m_stunConnectionPool;
    nx::network::stun::AbstractMessageHandler* m_messageHandler = nullptr;
    std::unique_ptr<detail::TunnelAuthorizer> m_tunnelAuthorizer;
    nx::network::http::tunneling::Server<nx::utils::stree::StringAttrDict> m_httpTunnelingServer;
    std::optional<std::chrono::milliseconds> m_inactivityTimeout;

    void createStunConnection(
        std::unique_ptr<AbstractStreamSocket> connection,
        nx::utils::stree::StringAttrDict attrs);

    void createStunConnection(
        std::unique_ptr<AbstractStreamSocket> connection,
        nx::network::http::RequestContext ctx);
};

} // namespace nx::network::stun
