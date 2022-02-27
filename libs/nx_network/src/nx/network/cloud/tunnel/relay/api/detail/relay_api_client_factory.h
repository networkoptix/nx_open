// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <memory>

#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

#include "../relay_api_client.h"

namespace nx::cloud::relay::api::detail {

using ClientFactoryFunction = std::unique_ptr<AbstractClient>(
    const nx::utils::Url& baseUrl,
    std::optional<int> forcedHttpTunnelType);

/**
 * Always instantiates client type with the highest priority.
 */
class NX_NETWORK_API ClientFactory:
    public nx::utils::BasicFactory<ClientFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<ClientFactoryFunction>;

public:
    ClientFactory();

    static ClientFactory& instance();

private:
    std::unique_ptr<AbstractClient> defaultFactoryFunction(
        const nx::utils::Url& baseUrl,
        std::optional<int> forcedHttpTunnelType);
};

} // namespace nx::cloud::relay::api::detail
