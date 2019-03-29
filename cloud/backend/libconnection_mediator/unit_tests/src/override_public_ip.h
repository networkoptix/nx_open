#pragma once

#include <nx/cloud/mediator/public_ip_discovery.h>

namespace nx::hpm::test {

class OverridePublicIp
{
public:
    OverridePublicIp()
    {
        PublicIpDiscovery::setDiscoverFunc(
            []() { return nx::network::HostAddress("127.0.0.1"); });
    }

    ~OverridePublicIp()
    {
        PublicIpDiscovery::setDiscoverFunc(nullptr);
    }
};

}  //namespace nx::hpm::test