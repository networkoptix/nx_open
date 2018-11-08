#pragma once

#include <gtest/gtest.h>

#include <nx/cloud/vms_gateway/test_support/vms_gateway_functional_test.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

class BasicComponentTest:
    public nx::cloud::gateway::VmsGatewayFunctionalTest,
    public ::testing::Test
{
};

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
