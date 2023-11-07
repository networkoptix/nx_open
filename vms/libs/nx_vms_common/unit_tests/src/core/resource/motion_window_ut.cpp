// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/motion_window.h>

namespace nx::vms::common::test {

TEST(QnMotionRegion, maskSize)
{
    ASSERT_TRUE(parseMotionRegion("5,0,0,44,32"));
    ASSERT_FALSE(parseMotionRegion("5,1,1,44,32"));
    ASSERT_FALSE(parseMotionRegion("5,-1,-1,43,31"));
    ASSERT_FALSE(parseMotionRegion("5,0,10,44,192"));
}

} // namespace nx::vms::common::test
