// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/network/rtsp/rtsp_types.h>

#include <nx/utils/datetime.h>

namespace nx::network::rtsp::header {

static bool operator==(const Range& a, const Range& b)
{
    return a.type == b.type &&
           a.startUs == b.startUs &&
           a.endUs == b.endUs;
}

} // namespace nx::network::rtsp::header

namespace nx::network::rtsp::test {

TEST(RtspHeaderRange, serialize_npt)
{
    using namespace header;

    EXPECT_EQ(
        (Range{Range::Type::npt, 45'000'000, 70'000'100}).serialize(),
        "npt=45-70.0001");

    EXPECT_EQ(
        (Range{Range::Type::npt, 55'000, DATETIME_NOW}).serialize(),
        "npt=0.055-now");

    EXPECT_EQ(
        (Range{Range::Type::npt, std::nullopt, 100'000'000}).serialize(),
        "npt=-100");

    EXPECT_EQ(
        (Range{Range::Type::npt, DATETIME_NOW, std::nullopt}).serialize(),
        "npt=now-");
}

TEST(RtspHeaderRange, parse_npt)
{
    using namespace header;

    {
        Range range;
        EXPECT_TRUE(range.parse("npt=45-70.0001"));
        EXPECT_EQ(range, (Range{Range::Type::npt, 45'000'000, 70'000'100}));
    }

    {
        Range range;
        EXPECT_TRUE(range.parse("npt=0.055-now"));
        EXPECT_EQ(range, (Range{Range::Type::npt, 55'000, DATETIME_NOW}));
    }

    {
        Range range;
        EXPECT_TRUE(range.parse("npt=-01:20:30.44"));
        EXPECT_EQ(range, (Range{Range::Type::npt, std::nullopt, 4'830'440'000}));
    }

    {
        Range range;
        EXPECT_TRUE(range.parse("npt=now-"));
        EXPECT_EQ(range, (Range{Range::Type::npt, DATETIME_NOW, std::nullopt}));
    }
}

TEST(RtspHeaderRange, serialize_clock)
{
    using namespace header;

    EXPECT_EQ(
        (Range{Range::Type::clock, 1626887246'005601, 1626889917'000000}).serialize(),
        "clock=20210721T170726.005601Z-20210721T175157Z");

    EXPECT_EQ(
        (Range{Range::Type::clock, 0, std::nullopt}).serialize(),
        "clock=19700101T000000Z-");
}

TEST(RtspHeaderRange, parse_clock)
{
    using namespace header;

    {
        Range range;
        EXPECT_TRUE(range.parse("clock=20210721T170726.005601Z-20210721T175157Z"));
        EXPECT_EQ(range, (Range{Range::Type::clock, 1626887246'005601, 1626889917'000000}));
    }

    {
        Range range;
        EXPECT_TRUE(range.parse("clock=19700101T000000Z-"));
        EXPECT_EQ(range, (Range{Range::Type::clock, 0, std::nullopt}));
    }

    {
        Range range;
        EXPECT_FALSE(range.parse("clock=19700101T000000Z-now"));
    }
}

TEST(RtspHeaderRange, serialize_nx_clock)
{
    using namespace header;

    EXPECT_EQ(
        (Range{Range::Type::nxClock, 1626887246'005601, 1626889917'000000}).serialize(),
        "clock=1626887246005601-1626889917000000");

    EXPECT_EQ(
        (Range{Range::Type::nxClock, 0, std::nullopt}).serialize(),
        "clock=0-");

    EXPECT_EQ(
        (Range{Range::Type::nxClock, std::nullopt, DATETIME_NOW}).serialize(),
        "clock=-now");
}

TEST(RtspHeaderRange, parse_nx_clock)
{
    using namespace header;

    {
        Range range;
        EXPECT_TRUE(range.parse("clock=1626887246005601-1626889917000000"));
        EXPECT_EQ(range, (Range{Range::Type::nxClock, 1626887246'005601, 1626889917'000000}));
    }

    {
        Range range;
        EXPECT_TRUE(range.parse("clock=0-"));
        EXPECT_EQ(range, (Range{Range::Type::nxClock, 0, std::nullopt}));
    }

    {
        Range range;
        EXPECT_TRUE(range.parse("clock=-now"));
        EXPECT_EQ(range, (Range{Range::Type::nxClock, std::nullopt, DATETIME_NOW}));
    }
}

} // namespace nx::network::rtsp::test
