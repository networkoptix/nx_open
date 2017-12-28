#pragma once

#include <boost/optional/optional.hpp>
#include <nx/utils/move_only_func.h>
#include <nx/network/socket_common.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

class PublicIpDiscoveryService
{
public:
    using DiscoverFunc = nx::utils::MoveOnlyFunc<boost::optional<network::HostAddress>()>;
    static void setDiscoverFunc(DiscoverFunc func);
    static boost::optional<network::HostAddress> get();
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
