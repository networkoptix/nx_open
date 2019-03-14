#include <gtest/gtest.h>

#include <nx/streaming/rtp/rtcp.h>

TEST(RtcpSenderReport, readWrite)
{
    using namespace nx::streaming::rtp;
    uint8_t buffer[RtcpSenderReport::kSize];
    RtcpSenderReport report;
    uint64_t timeUSec(1552500923000000);
    report.ntpTimestamp = timeUSec;
    ASSERT_EQ(report.write(buffer, RtcpSenderReport::kSize), RtcpSenderReport::kSize);
    RtcpSenderReport reportLoaded;
    ASSERT_TRUE(reportLoaded.read(buffer, RtcpSenderReport::kSize));
    ASSERT_EQ(reportLoaded.ntpTimestamp, timeUSec);

    ASSERT_FALSE(reportLoaded.read(buffer, 4));
    ASSERT_FALSE(reportLoaded.read(buffer, 0));
    ASSERT_FALSE(reportLoaded.read(nullptr, 0));
}
