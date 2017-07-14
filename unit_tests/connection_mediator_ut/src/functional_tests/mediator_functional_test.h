#pragma once

#include <gtest/gtest.h>

#include <test_support/mediator_functional_test.h>

namespace nx {
namespace hpm {
namespace test {

class MediatorFunctionalTest:
    public nx::hpm::MediatorFunctionalTest,
    public testing::Test
{
public:
};

} // namespace test
} // namespace hpm
} // namespace nx
