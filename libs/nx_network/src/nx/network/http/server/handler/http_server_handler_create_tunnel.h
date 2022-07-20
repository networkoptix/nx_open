// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>

#include "../abstract_http_request_handler.h"

namespace nx::network::http::server::handler {

using TunnelCreatedHandler =
    nx::utils::MoveOnlyFunc<void(
        std::unique_ptr<AbstractStreamSocket>,
        RequestPathParams /*REST request parameters values*/)>;

/**
 * Upgrades HTTP connection to the protocol specified.
 */
class NX_NETWORK_API CreateTunnelHandler:
    public RequestHandlerWithContext
{
public:
    /**
     * @param onTunnelCreated Invoked just after sending successful upgrade response
     * (101 Switching Protocol).
     */
    CreateTunnelHandler(
        const std::string& protocolToUpgradeTo,
        TunnelCreatedHandler onTunnelCreated);

    virtual void processRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    const std::string m_protocolToUpgradeTo;
    TunnelCreatedHandler m_onTunnelCreated;
};

} // namespace nx::network::http::server::handler
