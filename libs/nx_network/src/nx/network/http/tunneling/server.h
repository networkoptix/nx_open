#pragma once

#include <string>
#include <tuple>

#include <nx/utils/move_only_func.h>

#include "abstract_tunnel_authorizer.h"
#include "detail/connection_upgrade_tunnel_server.h"
#include "detail/get_post_tunnel_server.h"
#include "detail/multi_message_tunnel_server.h"
#include "detail/experimental_tunnel_server.h"
#include "../http_types.h"
#include "../server/rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::tunneling {

/**
 * Should be used for tunneling traffic inside an HTTP connection.
 * In general, such techniques are not compatible with some HTTP proxies and firewalls.
 * So, multiple tunneling methods are implemented:
 *
 * - GET/POST tunnel. ([Apple. Tunneling QuickTime RTSP and RTP over HTTP]).
 * The client sends GET and POST requests with a very large body inside a single TCP connection.
 * Server responds to GET with a very large body too. Server never responds to POST.
 * After receiving GET response client uses TCP connection for some other protocol.
 *
 * - Tunneling through HTTP connection upgrade. ([rfc7230, 6.7]).
 * Client and server utilize "Connection: Upgrade" and "Upgrade: ..."
 * headers to switch to a different protocol.
 * This method is similar to WebSocket.
 * Not compatible with some popular HTTP proxies (e.g., squid).
 */
template<typename ...ApplicationData>
class Server
{
public:
    using TunnelCreatedHandler = nx::utils::MoveOnlyFunc<void(
        std::unique_ptr<network::AbstractStreamSocket> /*connection*/,
        ApplicationData... /*applicationData*/)>;

    /**
     * @param tunnelAuthorizer If null, then any tunnel is considered authorized.
     */
    Server(
        TunnelCreatedHandler tunnelCreatedHandler,
        TunnelAuthorizer<ApplicationData...>* tunnelAuthorizer);

    /**
     * @return Reference to the created tunnel server.
     * Can be used for tuning specific for a particular tunnel type.
     */
    template<template<typename...> class SpecificTunnelServer>
    SpecificTunnelServer<ApplicationData...>& addTunnelServer();

    void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher);

private:
    TunnelCreatedHandler m_tunnelCreatedHandler;
    TunnelAuthorizer<ApplicationData...>* m_tunnelAuthorizer = nullptr;
    std::vector<std::unique_ptr<detail::AbstractTunnelServer<ApplicationData...>>> m_tunnelServers;

    void reportNewTunnel(
        std::unique_ptr<network::AbstractStreamSocket> /*connection*/,
        ApplicationData... /*applicationData*/);
};

//-------------------------------------------------------------------------------------------------

template<typename ...ApplicationData>
Server<ApplicationData...>::Server(
    TunnelCreatedHandler tunnelCreatedHandler,
    TunnelAuthorizer<ApplicationData...>* tunnelAuthorizer)
    :
    m_tunnelCreatedHandler(std::move(tunnelCreatedHandler)),
    m_tunnelAuthorizer(tunnelAuthorizer)
{
    addTunnelServer<detail::GetPostTunnelServer>();
    addTunnelServer<detail::ConnectionUpgradeTunnelServer>();
    addTunnelServer<detail::ExperimentalTunnelServer>();
    addTunnelServer<detail::MultiMessageTunnelServer>();
}

template<typename ...ApplicationData>
template<template<typename...> class SpecificTunnelServer>
SpecificTunnelServer<ApplicationData...>& Server<ApplicationData...>::addTunnelServer()
{
    auto tunnelServer = std::make_unique<SpecificTunnelServer<ApplicationData...>>(
        [this](auto... args) { reportNewTunnel(std::move(args)...); });
    if (m_tunnelAuthorizer)
        tunnelServer->setTunnelAuthorizer(m_tunnelAuthorizer);

    auto tunnelServerPtr = tunnelServer.get();
    m_tunnelServers.push_back(std::move(tunnelServer));
    return *tunnelServerPtr;
}

template<typename ...ApplicationData>
void Server<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    for (auto& tunnelServer: m_tunnelServers)
        tunnelServer->registerRequestHandlers(basePath, messageDispatcher);
}

template<typename ...ApplicationData>
void Server<ApplicationData...>::reportNewTunnel(
    std::unique_ptr<network::AbstractStreamSocket> connection,
    ApplicationData... applicationData)
{
    m_tunnelCreatedHandler(
        std::move(connection),
        std::move(applicationData)...);
}

} // namespace nx::network::http::tunneling
