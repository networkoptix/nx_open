#include "public_ip_discovery.h"

#include <nx/network/public_ip_discovery.h>

namespace nx::hpm {

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

static PublicIpDiscovery::DiscoverFunc discoverFunc;

void PublicIpDiscovery::setDiscoverFunc(DiscoverFunc func)
{
    discoverFunc.swap(func);
}

std::optional<network::HostAddress> PublicIpDiscovery::get()
{
    if (discoverFunc)
        return discoverFunc();

    return publicAddress();
}

} //namespace nx::hpm
