// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/rtp/rtcp.h>
#include <nx/rtp/rtcp_nack.h>

TEST(RtcpNackReport, readWrite)
{
    using namespace nx::rtp;

    // 1 sequence.
    {
        uint8_t data[] = {
            0x81, //< First byte.
            kRtcpGenericFeedback, //< Payload type.
            0x0, 0x3, //< Length.
            0x1, 0x2, 0x3, 0x4, //< Source SSRC.
            0x5, 0x6, 0x7, 0x8, //< Sender SSRC.
            0x10, 0x0, 0x0, 0x0 //< Lost: {0x1000}.
        };

        RtcpNackReport report;
        ASSERT_TRUE(report.parse(data, sizeof(data)));
        ASSERT_TRUE(report.header.version() == 2);
        ASSERT_TRUE(report.header.padding() == 0);
        ASSERT_TRUE(report.header.format() == kRtcpFeedbackFormatNack);
        ASSERT_TRUE(report.header.length == 3);
        ASSERT_TRUE(report.header.sourceSsrc == 0x01020304);
        ASSERT_TRUE(report.header.senderSsrc == 0x05060708);
        const auto sequences = report.getAllSequenceNumbers();
        ASSERT_TRUE(sequences.size() == 1);
        ASSERT_TRUE(sequences[0] == 0x1000);

        std::vector<uint8_t> output;
        output.resize(report.serialized());
        ASSERT_TRUE(report.serialize(output.data(), output.size()) == sizeof(data));
        ASSERT_TRUE(report.serialized() == sizeof(data));
        ASSERT_TRUE(memcmp(output.data(), data, sizeof(data)) == 0);

        std::vector<uint16_t> lostSequences;
        lostSequences.push_back(0x1000);
        auto buildedReport = buildNackReport(0x01020304, 0x05060708, lostSequences);
        ASSERT_EQ(buildedReport, report);
    }

    // 17 sequences.
    {
        uint8_t data[] = {
            0x81, //< First byte.
            kRtcpGenericFeedback, //< Payload type.
            0x0, 0x3, //< Length.
            0x1, 0x2, 0x3, 0x4, //< Source SSRC.
            0x5, 0x6, 0x7, 0x8, //< Sender SSRC.
            0x10, 0x0, 0xff, 0xff //< Lost: {0x1000 - 0x1010}.
        };

        RtcpNackReport report;
        ASSERT_TRUE(report.parse(data, sizeof(data)));
        // Skip trivial checks.
        const auto sequences = report.getAllSequenceNumbers();
        std::vector<uint16_t> expectedSequences;
        for (uint16_t i = 0x1000; i < 0x1011; ++i)
            expectedSequences.push_back(i);

        ASSERT_TRUE(sequences == expectedSequences);

        std::vector<uint8_t> output;
        output.resize(report.serialized());
        ASSERT_TRUE(report.serialize(output.data(), output.size()) == sizeof(data));
        ASSERT_TRUE(report.serialized() == sizeof(data));
        ASSERT_TRUE(memcmp(output.data(), data, sizeof(data)) == 0);

        std::vector<uint16_t> lostSequences;
        for (uint16_t i = 0x1000; i < 0x1011; ++i)
            lostSequences.push_back(i);
        auto buildedReport = buildNackReport(0x01020304, 0x05060708, lostSequences);
        ASSERT_EQ(buildedReport, report);
    }
    // 34 sequences in separate parts.
    {
        uint8_t data[] = {
            0x81, //< First byte.
            kRtcpGenericFeedback, //< Payload type.
            0x0, 0x4, //< Length.
            0x1, 0x2, 0x3, 0x4, //< Source SSRC.
            0x5, 0x6, 0x7, 0x8, //< Sender SSRC.
            0x10, 0x0, 0xff, 0xff, //< Lost: {0x1000 - 0x1010}.
            0x10, 0x30, 0xff, 0xff //< Lost: {0x1030 - 0x1040}.
        };

        RtcpNackReport report;
        ASSERT_TRUE(report.parse(data, sizeof(data)));
        // Skip trivial checks.
        const auto sequences = report.getAllSequenceNumbers();
        std::vector<uint16_t> expectedSequences;
        for (uint16_t i = 0x1000; i < 0x1011; ++i)
            expectedSequences.push_back(i);
        for (uint16_t i = 0x1030; i < 0x1041; ++i)
            expectedSequences.push_back(i);

        ASSERT_TRUE(sequences == expectedSequences);

        std::vector<uint8_t> output;
        output.resize(report.serialized());
        ASSERT_TRUE(report.serialize(output.data(), output.size()) == sizeof(data));
        ASSERT_TRUE(report.serialized() == sizeof(data));
        ASSERT_TRUE(memcmp(output.data(), data, sizeof(data)) == 0);

        std::vector<uint16_t> lostSequences;
        for (uint16_t i = 0x1000; i < 0x1011; ++i)
            lostSequences.push_back(i);
        for (uint16_t i = 0x1030; i < 0x1041; ++i)
            lostSequences.push_back(i);
        auto buildedReport = buildNackReport(0x01020304, 0x05060708, lostSequences);
        ASSERT_EQ(buildedReport, report);
    }
    // 2 sequences in separate parts.
    {
        uint8_t data[] = {
            0x81, //< First byte.
            kRtcpGenericFeedback, //< Payload type.
            0x0, 0x4, //< Length.
            0x1, 0x2, 0x3, 0x4, //< Source SSRC.
            0x5, 0x6, 0x7, 0x8, //< Sender SSRC.
            0x10, 0x0, 0x0, 0x0, //< Lost: {0x1000}.
            0x10, 0x30, 0x0, 0x0 //< Lost: {0x1030}.
        };

        RtcpNackReport report;
        ASSERT_TRUE(report.parse(data, sizeof(data)));
        // Skip trivial checks.
        const auto sequences = report.getAllSequenceNumbers();
        std::vector<uint16_t> expectedSequences;
        expectedSequences.push_back(0x1000);
        expectedSequences.push_back(0x1030);

        ASSERT_TRUE(sequences == expectedSequences);

        std::vector<uint8_t> output;
        output.resize(report.serialized());
        ASSERT_TRUE(report.serialize(output.data(), output.size()) == sizeof(data));
        ASSERT_TRUE(report.serialized() == sizeof(data));
        ASSERT_TRUE(memcmp(output.data(), data, sizeof(data)) == 0);

        std::vector<uint16_t> lostSequences;
        lostSequences.push_back(0x1000);
        lostSequences.push_back(0x1030);
        auto buildedReport = buildNackReport(0x01020304, 0x05060708, lostSequences);
        ASSERT_EQ(buildedReport, report);
    }
    // 2 non-consistent sequences.
    {
        uint8_t data[] = {
            0x81, //< First byte.
            kRtcpGenericFeedback, //< Payload type.
            0x0, 0x4, //< Length.
            0x1, 0x2, 0x3, 0x4, //< Source SSRC.
            0x5, 0x6, 0x7, 0x8, //< Sender SSRC.
            0x10, 0x1, 0x0, 0x0, //< Lost: {0x1001}.
            0x10, 0x0, 0x0, 0x0 //< Lost: {0x1000}.
        };

        RtcpNackReport report;
        ASSERT_TRUE(report.parse(data, sizeof(data)));
        // Skip trivial checks.
        const auto sequences = report.getAllSequenceNumbers();
        std::vector<uint16_t> expectedSequences;
        expectedSequences.push_back(0x1001);
        expectedSequences.push_back(0x1000);

        ASSERT_TRUE(sequences == expectedSequences);

        std::vector<uint8_t> output;
        output.resize(report.serialized());
        ASSERT_TRUE(report.serialize(output.data(), output.size()) == sizeof(data));
        ASSERT_TRUE(report.serialized() == sizeof(data));
        ASSERT_TRUE(memcmp(output.data(), data, sizeof(data)) == 0);

        std::vector<uint16_t> lostSequences;
        lostSequences.push_back(0x1001);
        lostSequences.push_back(0x1000);
        auto buildedReport = buildNackReport(0x01020304, 0x05060708, lostSequences);
        ASSERT_EQ(buildedReport, report);
    }
}
