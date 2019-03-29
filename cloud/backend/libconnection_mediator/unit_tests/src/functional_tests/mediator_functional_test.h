#pragma once

#include <gtest/gtest.h>

#include <nx/cloud/mediator/public_ip_discovery.h>
#include <nx/cloud/mediator/test_support/mediator_functional_test.h>

#include "../override_public_ip.h"

namespace nx {
namespace hpm {
namespace test {

class MediatorFunctionalTest:
    public nx::hpm::MediatorFunctionalTest,
    public testing::Test,
    OverridePublicIp
{
public:
};

} // namespace test
} // namespace hpm
} // namespace nx