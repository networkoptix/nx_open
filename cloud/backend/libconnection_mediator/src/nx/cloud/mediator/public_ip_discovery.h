#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/optional.h>
#include <nx/network/socket_common.h>

namespace nx::hpm {

class PublicIpDiscovery
{
public:
    using DiscoverFunc = nx::utils::MoveOnlyFunc<std::optional<network::HostAddress>()>;

    static void setDiscoverFunc(DiscoverFunc func);
    static std::optional<network::HostAddress> get();
};

} // namespace nx::hpm