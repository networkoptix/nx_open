/**********************************************************
* May 19, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <test_support/vms_gateway_functional_test.h>


namespace nx {
namespace cloud {
namespace gateway {
namespace test {

class VmsGatewayFunctionalTest
:
    public nx::cloud::gateway::VmsGatewayFunctionalTest,
    public ::testing::Test
{
};

}   // namespace test
}   // namespace gateway
}   // namespace cloud
}   // namespace nx
