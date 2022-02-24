// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <recording/time_period.h>

namespace test {

TEST(QnTimePeriodsTest, intersected)
{
    {
        // No intersection
        QnTimePeriod a(10, 1), b(15, 1);
        ASSERT_TRUE(a.intersected(b).isEmpty());
        ASSERT_TRUE(b.intersected(a).isEmpty());
    }

    {
        // B inside of A
        QnTimePeriod a(10, 20), b(15, 1);
        ASSERT_EQ(a.intersected(b), b);
        ASSERT_EQ(b.intersected(a), b);
    }

    {
        // B inside of infinite A
        QnTimePeriod a(10, QnTimePeriod::kInfiniteDuration), b(15, 1);
        ASSERT_EQ(a.intersected(b), b);
        ASSERT_EQ(b.intersected(a), b);
    }

    {
        // Infinite B inside of infinite A
        QnTimePeriod a(10, QnTimePeriod::kInfiniteDuration), b(15, QnTimePeriod::kInfiniteDuration);
        ASSERT_EQ(a.intersected(b), b);
        ASSERT_EQ(b.intersected(a), b);
    }

    {
        // Intersection of finite A and B
        QnTimePeriod a(10, 10), b(15, 10), e(15, 5);
        ASSERT_EQ(a.intersected(b), e);
        ASSERT_EQ(b.intersected(a), e);
    }

    {
        // Intersection of finite and infinite
        QnTimePeriod a(10, 10), b(15, QnTimePeriod::kInfiniteDuration), e(15, 5);
        ASSERT_EQ(a.intersected(b), e);
        ASSERT_EQ(b.intersected(a), e);
    }

    // TODO: 'touching' intervals, including an empty result
}

TEST(QnTimePeriodTest, fromInterval)
{
    ASSERT_EQ(QnTimePeriod(3, 5), QnTimePeriod::fromInterval(3, 8));
    ASSERT_EQ(QnTimePeriod(3, 5), QnTimePeriod::fromInterval(8, 3));
    ASSERT_EQ(QnTimePeriod(3, -1), QnTimePeriod::fromInterval(3, QnTimePeriod::kMaxTimeValue));
    ASSERT_EQ(QnTimePeriod(3, -1), QnTimePeriod::fromInterval(QnTimePeriod::kMaxTimeValue, 3));
}

} // namespace test
