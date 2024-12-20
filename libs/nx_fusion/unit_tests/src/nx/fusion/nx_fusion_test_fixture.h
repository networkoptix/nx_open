// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/utils/json/flags.h>

class QnFusionTestFixture: public testing::Test
{
protected:
    virtual void SetUp();
    virtual void TearDown();
};

namespace nx {

NX_REFLECTION_ENUM(TestFlag,
    Flag0 = 0x00,
    Flag1 = 0x01,
    Flag2 = 0x02,
    Flag4 = 0x04
)

Q_DECLARE_FLAGS(TestFlags, TestFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(TestFlags)

} // namespace nx
