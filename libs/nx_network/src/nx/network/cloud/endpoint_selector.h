// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nx/network/socket_common.h>
#include <nx/network/http/http_types.h>

namespace nx::network::cloud {

/**
 * Selects endpoint that is "best" due to some logic.
 */
class NX_NETWORK_API AbstractEndpointSelector
{
public:
    virtual ~AbstractEndpointSelector() {}

    /**
     * NOTE: Multiple calls are allowed.
     */
    virtual void selectBestEndpont(
        const std::string& moduleName,
        std::vector<SocketAddress> endpoints,
        std::function<void(nx::network::http::StatusCode::Value, SocketAddress)> handler) = 0;
};

class NX_NETWORK_API RandomEndpointSelector:
    public AbstractEndpointSelector
{
public:
    /**
     * @param handler Called directly in this method.
     */
    virtual void selectBestEndpont(
        const std::string& moduleName,
        std::vector<SocketAddress> endpoints,
        std::function<void(nx::network::http::StatusCode::Value, SocketAddress)> handler) override;
};

} // namespace nx::network::cloud
