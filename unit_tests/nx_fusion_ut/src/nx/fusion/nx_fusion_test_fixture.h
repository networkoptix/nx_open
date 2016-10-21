#pragma once

#include <gtest/gtest.h>

#include <nx/fusion/model_functions_fwd.h>

class QnFusionTestFixture: public testing::Test
{
protected:
    virtual void SetUp();
    virtual void TearDown();
};
