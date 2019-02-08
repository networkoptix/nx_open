#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>

#include "../abstract_http_request_handler.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace handler {

using TunnelCreatedHandler =
    nx::utils::MoveOnlyFunc<void(
        std::unique_ptr<AbstractStreamSocket>,
        RequestPathParams /*REST request parameters values*/)>;

/**
 * Upgrades HTTP connection to the protocol specified.
 */
class CreateTunnelHandler:
    public AbstractHttpRequestHandler
{
public:
    CreateTunnelHandler(
        const StringType& protocolToUpgradeTo,
        TunnelCreatedHandler onTunnelCreated)
        :
        m_protocolToUpgradeTo(protocolToUpgradeTo),
        m_onTunnelCreated(std::move(onTunnelCreated))
    {
    }

    virtual void processRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override
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

private:
    const StringType m_protocolToUpgradeTo;
    TunnelCreatedHandler m_onTunnelCreated;
};

} // namespace handler
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
