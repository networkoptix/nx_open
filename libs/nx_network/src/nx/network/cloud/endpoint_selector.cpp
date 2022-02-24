// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "endpoint_selector.h"

#include <cstdlib>

#include <nx/utils/random.h>

namespace nx::network::cloud {

void RandomEndpointSelector::selectBestEndpont(
    const std::string& /*moduleName*/,
    std::vector<SocketAddress> endpoints,
    std::function<void(nx::network::http::StatusCode::Value, SocketAddress)> handler)
{
    NX_ASSERT(!endpoints.empty());
    handler(
        nx::network::http::StatusCode::ok,
        nx::utils::random::choice(endpoints));
}

} // namespace nx::network::cloud
