#pragma once

#include <gtest/gtest.h>

#include <nx/fusion/model_functions_fwd.h>

class QnFusionTestFixture: public testing::Test
{
protected:
    virtual void SetUp();
    virtual void TearDown();
};

namespace nx {

enum TestFlag
{
    Flag0 = 0x00,
    Flag1 = 0x01,
    Flag2 = 0x02,
    Flag4 = 0x04,
};
Q_DECLARE_FLAGS(TestFlags, TestFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(TestFlags)

} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::TestFlag)(nx::TestFlags), (lexical))
