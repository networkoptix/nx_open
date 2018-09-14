#pragma once

#include <string>

#include <nx/utils/move_only_func.h>

#include "get_post_tunnel_processor.h"
#include "abstract_tunnel_authorizer.h"
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
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher,
        TunnelCreatedHandler tunnelCreatedHandler,
        TunnelAuthorizer<ApplicationData>* tunnelAuthorizer);

private:
    TunnelCreatedHandler m_tunnelCreatedHandler;
    TunnelAuthorizer<ApplicationData>* m_tunnelAuthorizer = nullptr;
    GetPostTunnelProcessor<ApplicationData> m_getPostTunnelProcessor;

    void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher);

    void reportNewTunnel(
        ApplicationData /*applicationData*/,
        std::unique_ptr<network::AbstractStreamSocket> /*connection*/);
};

//-------------------------------------------------------------------------------------------------

template<typename ApplicationData>
Server<ApplicationData>::Server(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher,
    TunnelCreatedHandler tunnelCreatedHandler,
    TunnelAuthorizer<ApplicationData>* tunnelAuthorizer)
    :
    m_tunnelCreatedHandler(std::move(tunnelCreatedHandler)),
    m_tunnelAuthorizer(tunnelAuthorizer),
    m_getPostTunnelProcessor(std::bind(&Server::reportNewTunnel, this, 
        std::placeholders::_1, std::placeholders::_2))
{
    registerRequestHandlers(basePath, messageDispatcher);

    if (m_tunnelAuthorizer)
        m_getPostTunnelProcessor.setTunnelAuthorizer(m_tunnelAuthorizer);
}

template<typename ApplicationData>
void Server<ApplicationData>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    m_getPostTunnelProcessor.registerRequestHandlers(
        basePath,
        messageDispatcher);
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
