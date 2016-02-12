
#pragma once

#include <gtest/gtest.h>

#include <test_support/mediator_functional_test.h>


namespace nx {
namespace hpm {
namespace test {

class MediatorFunctionalTest
:
    public nx::hpm::MediatorFunctionalTest,
    public testing::Test
{
public:
    void startAndWaitUntilStarted()
    {
        ASSERT_TRUE(nx::hpm::MediatorFunctionalTest::startAndWaitUntilStarted());
    }
    void waitUntilStarted()
    {
        ASSERT_TRUE(nx::hpm::MediatorFunctionalTest::waitUntilStarted());
    }
};

}   //test
}   //hpm
}   //nx
