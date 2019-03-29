#pragma once

#include <gtest/gtest.h>

#include <nx/cloud/mediator/public_ip_discovery.h>
#include <nx/cloud/mediator/test_support/mediator_functional_test.h>

namespace nx {
namespace hpm {
namespace test {

class MediatorFunctionalTest:
    public nx::hpm::MediatorFunctionalTest,
    public testing::Test
{
public:
    MediatorFunctionalTest()
    {
        PublicIpDiscovery::setDiscoverFunc(
            []() { return nx::network::HostAddress("127.0.0.1"); });
    }

    ~MediatorFunctionalTest()
    {
        PublicIpDiscovery::setDiscoverFunc(nullptr);
    }
};

} // namespace test
} // namespace hpm
} // namespace nx