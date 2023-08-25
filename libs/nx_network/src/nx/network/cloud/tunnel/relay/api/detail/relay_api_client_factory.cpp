// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_api_client_factory.h"

#include <algorithm>

#include <nx/utils/std/algorithm.h>

#include "relay_api_client_over_http_tunnel.h"

namespace nx::cloud::relay::api::detail {

ClientFactory::ClientFactory():
    base_type([this](auto&&... args) {
        return defaultFactoryFunction(std::forward<decltype(args)>(args)...);
    })
{
}

ClientFactory& ClientFactory::instance()
{
    static ClientFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractClient> ClientFactory::defaultFactoryFunction(
    const utils::Url& baseUrl,
    std::optional<int> forcedHttpTunnelType)
{
    return std::make_unique<ClientOverHttpTunnel>(baseUrl, forcedHttpTunnelType);
}

} // namespace nx::cloud::relay::api::detail
