// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <recording/helpers/timestamp_stitcher.h>

namespace std::chrono {

void PrintTo(microseconds us, ::std::ostream* os) { (*os) << us.count() << "us"; }
void PrintTo(milliseconds ms, ::std::ostream* os) { (*os) << ms.count() << "ms"; }

}

using namespace std::chrono;
using namespace std::chrono_literals;

TEST(TimestampStitcher, SimpleGap)
{
    nx::recording::helpers::TimestampStitcher stitcher;
    ASSERT_EQ(0ms, stitcher.process(0ms));
    ASSERT_EQ(25ms, stitcher.process(25ms));
    ASSERT_EQ(50ms, stitcher.process(50ms));
    ASSERT_EQ(75ms, stitcher.process(10000ms));
    ASSERT_EQ(100ms, stitcher.process(10025ms));
}

TEST(TimestampStitcher, BackwardGap)
{
    nx::recording::helpers::TimestampStitcher stitcher;
    ASSERT_EQ(1000ms, stitcher.process(1000ms));
    ASSERT_EQ(1025ms, stitcher.process(1025ms));
    ASSERT_EQ(1050ms, stitcher.process(1050ms));
    ASSERT_EQ(1075ms, stitcher.process(500ms));
    ASSERT_EQ(1100ms, stitcher.process(525ms));
}

TEST(TimestampStitcher, SimpleGapUtc)
{
    nx::recording::helpers::TimestampStitcher stitcher;
    ASSERT_EQ(1630509000000ms, stitcher.process(1630509000000ms));
    ASSERT_EQ(1630509000025ms, stitcher.process(1630509000025ms));
    ASSERT_EQ(1630509000050ms, stitcher.process(1630509000050ms));
    ASSERT_EQ(1630509000075ms, stitcher.process(1630509510000ms));
    ASSERT_EQ(1630509000100ms, stitcher.process(1630509510025ms));
}

TEST(TimestampStitcher, OnlyGaps)
{
    nx::recording::helpers::TimestampStitcher stitcher;
    ASSERT_EQ(0ms, stitcher.process(0s));
    ASSERT_EQ(33ms, stitcher.process(20s));
    ASSERT_EQ(66ms, stitcher.process(40s));
    ASSERT_EQ(99ms, stitcher.process(60s));
}
