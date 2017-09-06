#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>

#include "../abstract_http_request_handler.h"

namespace nx_http {
namespace server {
namespace handler {

using TunnelCreatedHandler = 
    nx::utils::MoveOnlyFunc<void(
        std::unique_ptr<AbstractStreamSocket>,
        std::vector<StringType> /*REST request parameters values*/)>;

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
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request request,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler) override
    {
        using namespace std::placeholders;

        auto upgradeIter = request.headers.find("Upgrade");
        if (upgradeIter == request.headers.end() ||
            upgradeIter->second != m_protocolToUpgradeTo)
        {
            return completionHandler(nx_http::StatusCode::badRequest);
        }

        nx_http::RequestResult requestResult(nx_http::StatusCode::switchingProtocols);
        requestResult.connectionEvents.onResponseHasBeenSent = 
            [onTunnelCreated = std::move(m_onTunnelCreated), restParams = requestPathParams()](
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
} // namespace nx_http
