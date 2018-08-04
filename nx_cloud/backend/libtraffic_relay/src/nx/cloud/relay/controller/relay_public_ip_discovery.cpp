#include <nx/network/public_ip_discovery.h>
#include "relay_public_ip_discovery.h"

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

namespace {

std::optional<network::HostAddress> publicAddress()
{
    network::PublicIPDiscovery ipDiscovery;

    ipDiscovery.update();
    ipDiscovery.waitForFinished();
    auto addr = ipDiscovery.publicIP();

    if (!addr.isNull())
        return network::HostAddress(addr.toString());

    return std::nullopt;
}

} // namespace

static PublicIpDiscoveryService::DiscoverFunc discoverFunc;

void PublicIpDiscoveryService::setDiscoverFunc(DiscoverFunc func)
{
    discoverFunc.swap(func);
}

std::optional<network::HostAddress> PublicIpDiscoveryService::get()
{
    if (discoverFunc)
        return discoverFunc();

    return publicAddress();
}

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
