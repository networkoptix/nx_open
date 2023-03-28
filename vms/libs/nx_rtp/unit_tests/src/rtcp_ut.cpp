// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/rtp/rtcp.h>

TEST(RtcpSenderReport, readWrite)
{
    using namespace nx::rtp;
    {
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


    {
        uint8_t data[] = {0x80, 0xc8, 0x00, 0x06, 0x79, 0xcd, 0x66, 0x0c, 0xbc, 0x26, 0xaf, 0xd5,
            0x8d, 0x7a, 0x78, 0x6c, 0xbf, 0x9f, 0xbd, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00};
        RtcpSenderReport report;
        ASSERT_TRUE(report.read(data, sizeof(data)));
        ASSERT_EQ(report.ntpTimestamp, 947663189552650);
    }
}
