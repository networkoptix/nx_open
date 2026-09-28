// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QtEndian>

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

TEST(RtcpReceiverReport, usesProvidedSsrc)
{
    using namespace nx::rtp;

    for (const uint32_t ssrc: {0u, 0x12345678u})
    {
        uint8_t buffer[64];
        const int size = buildClientRtcpReport(buffer, sizeof(buffer), ssrc);
        ASSERT_GT(size, kRtcpReceiverReportLength);

        const uint32_t receiverReportSsrc = qFromBigEndian<uint32_t>(buffer + 4);
        EXPECT_EQ(receiverReportSsrc, ssrc);
        EXPECT_EQ(
            getRtcpSsrc(buffer + kRtcpReceiverReportLength, size - kRtcpReceiverReportLength),
            ssrc);
    }
}

TEST(RtcpReceiverReport, usesProvidedCname)
{
    using namespace nx::rtp;

    constexpr uint32_t kSsrc = 0x12345678;
    const std::string cname = "rtsp-client";
    uint8_t buffer[64];
    const int size = buildClientRtcpReport(buffer, sizeof(buffer), kSsrc, cname);
    const int cnameOffset = kRtcpReceiverReportLength + 8;
    ASSERT_GE(size, cnameOffset + 2 + (int) cname.size());
    EXPECT_EQ(buffer[cnameOffset], 1); //< CNAME SDES item.
    EXPECT_EQ(buffer[cnameOffset + 1], cname.size());
    EXPECT_EQ(
        std::string(reinterpret_cast<const char*>(buffer + cnameOffset + 2), cname.size()), cname);
}
