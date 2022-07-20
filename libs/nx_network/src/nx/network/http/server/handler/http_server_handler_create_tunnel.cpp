// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_handler_create_tunnel.h"

#include "../http_server_connection.h"

namespace nx::network::http::server::handler {

CreateTunnelHandler::CreateTunnelHandler(
    const std::string& protocolToUpgradeTo,
    TunnelCreatedHandler onTunnelCreated)
    :
    m_protocolToUpgradeTo(protocolToUpgradeTo),
    m_onTunnelCreated(std::move(onTunnelCreated))
{
}

void CreateTunnelHandler::processRequest(
    RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    const auto& request = requestContext.request;

    auto upgradeIter = request.headers.find("Upgrade");
    if (upgradeIter == request.headers.end() ||
        upgradeIter->second != m_protocolToUpgradeTo)
    {
        return completionHandler(StatusCode::badRequest);
    }

    RequestResult requestResult(StatusCode::switchingProtocols);
    requestResult.connectionEvents.onResponseHasBeenSent =
        [onTunnelCreated = std::move(m_onTunnelCreated),
            restParams = std::exchange(requestContext.requestPathParams, {})](
                HttpServerConnection* httpConnection)
        {
            onTunnelCreated(
                httpConnection->takeSocket(),
                std::move(restParams));
        };
    completionHandler(std::move(requestResult));
}

} // namespace nx::network::http::server::handler
