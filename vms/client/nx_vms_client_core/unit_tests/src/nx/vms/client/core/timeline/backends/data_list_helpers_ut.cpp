// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/core/timeline/backends/data_list_helpers.h>

#include "test_backends.h"

namespace nx::vms::client::core::timeline {
namespace test {

using namespace std::chrono;

class TimelineDataListHelpersTest:
    public ::testing::Test,
    public DataListHelpers<TestDataList>
{
public:
    const TestDataList data{
        {5ms, 'A'},
        {5ms, 'B'},
        {4ms, 'C'},
        {4ms, 'D'},
        {4ms, 'E'},
        {3ms, 'F'},
        {3ms, 'G'},
        {2ms, 'H'},
        {1ms, 'I'},
        {1ms, 'J'}};

    static TestDataList sliceViaClip(
        const TestDataList& data, const QnTimePeriod& period, int limit)
    {
        auto d = data;
        TimelineDataListHelpersTest::clip(d, period, limit);
        return d;
    }
};

TEST_F(TimelineDataListHelpersTest, predicate)
{
    EXPECT_TRUE(std::is_sorted(data.begin(), data.end(), greater));
    ASSERT_EQ(payload(data), "ABCDEFGHIJ"); //< A quick self-test.
}

TEST_F(TimelineDataListHelpersTest, coveredPeriod)
{
    EXPECT_EQ(coveredPeriod({}), QnTimePeriod());
    EXPECT_EQ(coveredPeriod(data), interval(5ms, 1ms));
}

TEST_F(TimelineDataListHelpersTest, sliceAndClip)
{
    std::vector<std::function<TestDataList(const TestDataList&, const QnTimePeriod&, int)>> funcs{
        &TimelineDataListHelpersTest::slice, &TimelineDataListHelpersTest::sliceViaClip};

    for (const auto& func: funcs)
    {
        // Full interval, uncropped.
        EXPECT_EQ(payload(func(data, interval(9ms, 0ms), kUnlimited)), "ABCDEFGHIJ");
        EXPECT_EQ(payload(func(data, interval(5ms, 1ms), kUnlimited)), "ABCDEFGHIJ");

        // Full interval, cropped.
        EXPECT_EQ(payload(func(data, interval(9ms, 0ms), /*limit*/ 1)), "A");
        EXPECT_EQ(payload(func(data, interval(9ms, 0ms), /*limit*/ 5)), "ABCDE");
        EXPECT_EQ(payload(func(data, interval(9ms, 0ms), /*limit*/ 9)), "ABCDEFGHI");

        // Oldest end of the interval, uncropped.
        EXPECT_EQ(payload(func(data, interval(1ms, 0ms), kUnlimited)), "IJ");
        EXPECT_EQ(payload(func(data, interval(1ms, 1ms), kUnlimited)), "IJ");
        EXPECT_EQ(payload(func(data, interval(2ms, 1ms), kUnlimited)), "HIJ");

        // Oldest end of the interval, cropped.
        EXPECT_EQ(payload(func(data, interval(1ms, 0ms), /*limit*/ 1)), "I");
        EXPECT_EQ(payload(func(data, interval(1ms, 1ms), /*limit*/ 1)), "I");
        EXPECT_EQ(payload(func(data, interval(2ms, 1ms), /*limit*/ 2)), "HI");

        // Newest end of the interval, uncropped.
        EXPECT_EQ(payload(func(data, interval(9ms, 5ms), kUnlimited)), "AB");
        EXPECT_EQ(payload(func(data, interval(5ms, 5ms), kUnlimited)), "AB");
        EXPECT_EQ(payload(func(data, interval(5ms, 4ms), kUnlimited)), "ABCDE");

        // Newest end of the interval, cropped.
        EXPECT_EQ(payload(func(data, interval(9ms, 5ms), /*limit*/ 1)), "A");
        EXPECT_EQ(payload(func(data, interval(5ms, 5ms), /*limit*/ 1)), "A");
        EXPECT_EQ(payload(func(data, interval(5ms, 4ms), /*limit*/ 3)), "ABC");

        // Middle of the interval, uncropped.
        EXPECT_EQ(payload(func(data, interval(4ms, 3ms), kUnlimited)), "CDEFG");
        EXPECT_EQ(payload(func(data, interval(4ms, 4ms), kUnlimited)), "CDE");

        // Middle of the interval, cropped.
        EXPECT_EQ(payload(func(data, interval(4ms, 3ms), /*limit*/ 1)), "C");
        EXPECT_EQ(payload(func(data, interval(4ms, 3ms), /*limit*/ 3)), "CDE");
        EXPECT_EQ(payload(func(data, interval(4ms, 3ms), /*limit*/ 4)), "CDEF");
        EXPECT_EQ(payload(func(data, interval(4ms, 4ms), /*limit*/ 1)), "C");

        // Outside of the interval.
        EXPECT_EQ(payload(func(data, interval(0ms, 0ms), kUnlimited)), "");
        EXPECT_EQ(payload(func(data, interval(9ms, 6ms), kUnlimited)), "");

        // Empty source list.
        const TestDataList emptyData;
        EXPECT_EQ(payload(func(emptyData, interval(0ms, 9ms), kUnlimited)), "");

        // One-element source list.
        const TestDataList oneElementData{{5ms, 'A'}};
        EXPECT_EQ(payload(func(oneElementData, interval(9ms, 0ms), kUnlimited)), "A");
        EXPECT_EQ(payload(func(oneElementData, interval(9ms, 0ms), 1)), "A");
        EXPECT_EQ(payload(func(oneElementData, interval(4ms, 0ms), kUnlimited)), "");
        EXPECT_EQ(payload(func(oneElementData, interval(9ms, 6ms), kUnlimited)), "");
        EXPECT_EQ(payload(func(oneElementData, interval(5ms, 5ms), kUnlimited)), "A");
        EXPECT_EQ(payload(func(oneElementData, interval(5ms, 5ms), 1)), "A");
    }
}

} // namespace test
} // namespace nx::vms::client::core::timeline
