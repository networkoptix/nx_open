#include <gtest/gtest.h>

#include <recording/time_period.h>

namespace test {

TEST( QnTimePeriodsTest, intersected )
{
    QnTimePeriod result;

    {
        // No intersection
        QnTimePeriod a(10, 1), b(15, 1);
        result = a.intersected(b);
        ASSERT_EQ(result.isEmpty(), true);

        result = b.intersected(a);
        ASSERT_EQ(result.isEmpty(), true);
    }

    {
        // B inside of A
        QnTimePeriod a(10, 20), b(15, 1);
        result = a.intersected(b);
        ASSERT_EQ(result, b);

        result = b.intersected(a);
        ASSERT_EQ(result, b);
    }

    {
        // B inside of infinite A
        QnTimePeriod a(10, QnTimePeriod::kInfiniteDuration), b(15, 1);
        result = a.intersected(b);
        ASSERT_EQ(result, b);

        result = b.intersected(a);
        ASSERT_EQ(result, b);
    }

    {
        // Infinite B inside of infinite A
        QnTimePeriod a(10, QnTimePeriod::kInfiniteDuration), b(15, QnTimePeriod::kInfiniteDuration);
        result = a.intersected(b);
        ASSERT_EQ(result, b);

        result = b.intersected(a);
        ASSERT_EQ(result, b);
    }

    {
        // Intersection of finite A and B
        QnTimePeriod a(10, 10), b(15, 10), e(15, 5);
        result = a.intersected(b);
        ASSERT_EQ(result, e);

        result = b.intersected(a);
        ASSERT_EQ(result, e);
    }

    {
        // Intersection of finite and infinite
        QnTimePeriod a(10, 10), b(15, QnTimePeriod::kInfiniteDuration), e(15, 5);
        result = a.intersected(b);
        ASSERT_EQ(result, e);

        result = b.intersected(a);
        ASSERT_EQ(result, e);
    }

    // TODO: 'touching' intervals, including an empty result
}

} // namespace test
