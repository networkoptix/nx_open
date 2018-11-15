#include <gtest/gtest.h>

#include <nx/utils/interval.h>

using Interval = nx::utils::Interval<int>;

TEST(Interval, basic)
{
    ASSERT_FALSE(Interval(0, 1).isEmpty());
    ASSERT_TRUE(Interval(1, 0).isEmpty());
    ASSERT_TRUE(Interval(0, 0).isEmpty());

    ASSERT_TRUE(Interval(0, 10).contains(0));
    ASSERT_FALSE(Interval(0, 10).contains(10));

    ASSERT_EQ(Interval(0, 0), Interval(10, 10)); //< Empty intervals are equal.
}

TEST(Interval, intersection)
{
    ASSERT_TRUE(Interval(0, 9).intersected(Interval(10, 20)).isEmpty());
    ASSERT_TRUE(Interval(0, 10).intersected(Interval(10, 20)).isEmpty());
    ASSERT_TRUE(Interval(11, 11).intersected(Interval(10, 20)).isEmpty());
    ASSERT_TRUE(Interval(20, 30).intersected(Interval(10, 20)).isEmpty());
    ASSERT_TRUE(Interval(21, 30).intersected(Interval(10, 20)).isEmpty());

    ASSERT_EQ(Interval(0, 15).intersected(Interval(10, 20)), Interval(10, 15));
    ASSERT_EQ(Interval(11, 15).intersected(Interval(10, 20)), Interval(11, 15));
    ASSERT_EQ(Interval(15, 30).intersected(Interval(10, 20)), Interval(15, 20));
}

TEST(Interval, expansion)
{
    ASSERT_EQ(Interval(10, 20).expanded(15), Interval(10, 20));
    ASSERT_EQ(Interval(10, 20).expanded(20), Interval(10, 21));
    ASSERT_EQ(Interval(10, 20).expanded(5), Interval(5, 20));
    ASSERT_EQ(Interval(0, 0).expanded(5), Interval(5));

    ASSERT_EQ(Interval(0, 15).expanded(Interval(10, 20)), Interval(0, 20));
    ASSERT_EQ(Interval(0, 10).expanded(Interval(15, 20)), Interval(0, 20));
    ASSERT_EQ(Interval(10, 20).expanded(Interval(0, 15)), Interval(0, 20));
    ASSERT_EQ(Interval(10, 20).expanded(Interval(0, 10)), Interval(0, 20));
    ASSERT_EQ(Interval(0, 20).expanded(Interval(10, 15)), Interval(0, 20));

    ASSERT_EQ(Interval(10, 20).expanded(Interval(0, 0)), Interval(10, 20));
    ASSERT_EQ(Interval(0, 0).expanded(Interval(10, 20)), Interval(10, 20));
}

TEST(Interval, truncation)
{
    ASSERT_EQ(Interval(10, 20).truncatedLeft(0), Interval(10, 20));
    ASSERT_EQ(Interval(10, 20).truncatedLeft(10), Interval(10, 20));
    ASSERT_EQ(Interval(10, 20).truncatedLeft(15), Interval(15, 20));
    ASSERT_EQ(Interval(10, 20).truncatedLeft(19), Interval(19));
    ASSERT_TRUE(Interval(10, 20).truncatedLeft(20).isEmpty());
    ASSERT_TRUE(Interval(10, 20).truncatedLeft(30).isEmpty());

    ASSERT_EQ(Interval(10, 20).truncatedRight(30), Interval(10, 20));
    ASSERT_EQ(Interval(10, 20).truncatedRight(20), Interval(10, 20));
    ASSERT_EQ(Interval(10, 20).truncatedRight(15), Interval(10, 15));
    ASSERT_EQ(Interval(10, 20).truncatedRight(11), Interval(10));
    ASSERT_TRUE(Interval(10, 20).truncatedRight(10).isEmpty());
    ASSERT_TRUE(Interval(10, 20).truncatedRight(0).isEmpty());
}

TEST(Interval, shift)
{
    ASSERT_EQ(Interval(10, 20).shifted(10), Interval(20, 30));
    ASSERT_EQ(Interval(10, 20).shifted(-20), Interval(-10, 0));
    ASSERT_EQ(Interval(10, 20).shifted(0), Interval(10, 20));
    ASSERT_TRUE(Interval().shifted(10).isEmpty());
}
