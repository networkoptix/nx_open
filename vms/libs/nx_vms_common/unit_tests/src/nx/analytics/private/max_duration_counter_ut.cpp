// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/analytics/private/max_duration_counter.h>

namespace nx::analytics {

namespace test {

TEST(MaxDurationCounter, pushChangeSize)
{
    MaxDurationCounter counter;
    ASSERT_EQ(counter.size(), 0);

    counter.push(10);
    ASSERT_EQ(counter.size(), 1);
    ASSERT_EQ(counter.max(), 10);
}

TEST(MaxDurationCounter, popChangeSize)
{
    MaxDurationCounter counter;
    counter.push(20);
    counter.push(10);
    ASSERT_EQ(counter.size(), 2);
    ASSERT_EQ(counter.max(), 20);

    counter.pop();
    ASSERT_EQ(counter.size(), 1);
    ASSERT_EQ(counter.max(), 10);

    counter.push(30);
    ASSERT_EQ(counter.size(), 2);
    ASSERT_EQ(counter.max(), 30);
    counter.pop(2);
    ASSERT_EQ(counter.size(), 0);
}

} // namespace test

} // namespace nx::analytics
