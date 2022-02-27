// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
