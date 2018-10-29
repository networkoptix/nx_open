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

    void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher);

    detail::GetPostTunnelServer<ApplicationData...>& getPostTunnelServer();
    detail::ConnectionUpgradeTunnelServer<ApplicationData...>& connectionUpgradeServer();
    detail::ExperimentalTunnelServer<ApplicationData...>& experimentalServer();
    detail::MultiMessageTunnelServer<ApplicationData...>& multiMessageTunnelServer();

private:
    TunnelCreatedHandler m_tunnelCreatedHandler;
    TunnelAuthorizer<ApplicationData...>* m_tunnelAuthorizer = nullptr;

    // NOTE: On the one hand, it would be good to have here a list of abstract factories.
    // On the other hand, modules that use these tunnels want to be able to configure 
    // a specific tunnel server in a specific way (at least, change paths for backward
    // compatibility).
    // So, this class serves as a place where specific tunnel servers are registered.
    // It also hides tunnel specifics from outer world while, at the same time, providing 
    // a way to tune a speficic tunnel in a specific way.

    detail::GetPostTunnelServer<ApplicationData...> m_getPostTunnelServer;
    detail::ConnectionUpgradeTunnelServer<ApplicationData...> m_connectionUpgradeServer;
    detail::ExperimentalTunnelServer<ApplicationData...> m_experimentalServer;
    detail::MultiMessageTunnelServer<ApplicationData...> m_multiMessageTunnelServer;

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
    m_tunnelAuthorizer(tunnelAuthorizer),
    m_getPostTunnelServer([this](auto... args) { reportNewTunnel(std::move(args)...); }),
    m_connectionUpgradeServer([this](auto... args) { reportNewTunnel(std::move(args)...); }),
    m_experimentalServer([this](auto... args) { reportNewTunnel(std::move(args)...); }),
    m_multiMessageTunnelServer([this](auto... args) { reportNewTunnel(std::move(args)...); })
{
    if (m_tunnelAuthorizer)
    {
        m_getPostTunnelServer.setTunnelAuthorizer(m_tunnelAuthorizer);
        m_connectionUpgradeServer.setTunnelAuthorizer(m_tunnelAuthorizer);
        m_experimentalServer.setTunnelAuthorizer(m_tunnelAuthorizer);
        m_multiMessageTunnelServer.setTunnelAuthorizer(m_tunnelAuthorizer);
    }
}

template<typename ...ApplicationData>
void Server<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    m_getPostTunnelServer.registerRequestHandlers(basePath, messageDispatcher);
    m_connectionUpgradeServer.registerRequestHandlers(basePath, messageDispatcher);
    m_experimentalServer.registerRequestHandlers(basePath, messageDispatcher);
    m_multiMessageTunnelServer.registerRequestHandlers(basePath, messageDispatcher);
}

template<typename ...ApplicationData>
detail::GetPostTunnelServer<ApplicationData...>& 
    Server<ApplicationData...>::getPostTunnelServer()
{
    return m_getPostTunnelServer;
}

template<typename ...ApplicationData>
detail::ConnectionUpgradeTunnelServer<ApplicationData...>& 
    Server<ApplicationData...>::connectionUpgradeServer()
{
    return m_connectionUpgradeServer;
}

template<typename ...ApplicationData>
detail::ExperimentalTunnelServer<ApplicationData...>&
    Server<ApplicationData...>::experimentalServer()
{
    return m_experimentalServer;
}

template<typename ...ApplicationData>
detail::MultiMessageTunnelServer<ApplicationData...>&
    Server<ApplicationData...>::multiMessageTunnelServer()
{
    return m_multiMessageTunnelServer;
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
