#pragma once

#include <string>

#include <nx/utils/move_only_func.h>

#include "abstract_tunnel_authorizer.h"
#include "detail/connection_upgrade_tunnel_server.h"
#include "detail/get_post_tunnel_server.h"
#include "../http_types.h"
#include "../server/rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::tunneling {

template<typename ApplicationData>
class Server
{
public:
    using TunnelCreatedHandler = nx::utils::MoveOnlyFunc<void(
        ApplicationData /*applicationData*/,
        std::unique_ptr<network::AbstractStreamSocket> /*connection*/)>;

    /**
     * @param tunnelAuthorizer If null, then any tunnel is considered authorized.
     */
    Server(
        TunnelCreatedHandler tunnelCreatedHandler,
        TunnelAuthorizer<ApplicationData>* tunnelAuthorizer);

    void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher);

    detail::GetPostTunnelServer<ApplicationData>& getPostTunnelServer();
    detail::ConnectionUpgradeTunnelServer<ApplicationData>& connectionUpgradeServer();

private:
    TunnelCreatedHandler m_tunnelCreatedHandler;
    TunnelAuthorizer<ApplicationData>* m_tunnelAuthorizer = nullptr;
    detail::GetPostTunnelServer<ApplicationData> m_getPostTunnelServer;
    detail::ConnectionUpgradeTunnelServer<ApplicationData> m_connectionUpgradeServer;

    void reportNewTunnel(
        ApplicationData /*applicationData*/,
        std::unique_ptr<network::AbstractStreamSocket> /*connection*/);
};

//-------------------------------------------------------------------------------------------------

template<typename ApplicationData>
Server<ApplicationData>::Server(
    TunnelCreatedHandler tunnelCreatedHandler,
    TunnelAuthorizer<ApplicationData>* tunnelAuthorizer)
    :
    m_tunnelCreatedHandler(std::move(tunnelCreatedHandler)),
    m_tunnelAuthorizer(tunnelAuthorizer),
    m_getPostTunnelServer(std::bind(&Server::reportNewTunnel, this, 
        std::placeholders::_1, std::placeholders::_2)),
    m_connectionUpgradeServer(std::bind(&Server::reportNewTunnel, this,
        std::placeholders::_1, std::placeholders::_2))
{
    if (m_tunnelAuthorizer)
    {
        m_getPostTunnelServer.setTunnelAuthorizer(m_tunnelAuthorizer);
        m_connectionUpgradeServer.setTunnelAuthorizer(m_tunnelAuthorizer);
    }
}

template<typename ApplicationData>
void Server<ApplicationData>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    m_getPostTunnelServer.registerRequestHandlers(basePath, messageDispatcher);
    m_connectionUpgradeServer.registerRequestHandlers(basePath, messageDispatcher);
}

template<typename ApplicationData>
detail::GetPostTunnelServer<ApplicationData>& 
    Server<ApplicationData>::getPostTunnelServer()
{
    return m_getPostTunnelServer;
}

template<typename ApplicationData>
detail::ConnectionUpgradeTunnelServer<ApplicationData>& 
    Server<ApplicationData>::connectionUpgradeServer()
{
    return m_connectionUpgradeServer;
}

template<typename ApplicationData>
void Server<ApplicationData>::reportNewTunnel(
    ApplicationData applicationData,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    m_tunnelCreatedHandler(
        std::move(applicationData),
        std::move(connection));
}

} // namespace nx::network::http::tunneling
