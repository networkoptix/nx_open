// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <nx/webrtc/webrtc_twcc.h>

namespace nx::webrtc::test {

using namespace std::chrono;

// These tests pin the exact on-the-wire bytes produced by the TWCC serializer
// so the big-endian writer refactor can be verified byte-for-byte.

TEST(Twcc, parseOneByteExtension)
{
    // One-byte RFC 8285 block: id=3, len field=1 (=> 2 data bytes), seq=0x1234.
    const uint8_t data[] = {0x31, 0x12, 0x34};
    const auto seq = parseTransportCcSeq(0xbede, data, sizeof(data), /*transportCcId*/ 3);
    ASSERT_TRUE(seq.has_value());
    EXPECT_EQ(*seq, 0x1234);
}

TEST(Twcc, parseSkipsPaddingAndOtherIds)
{
    // 0x00 padding byte, then an unrelated id=1 (len 1 byte), then our id=3.
    const uint8_t data[] = {0x00, 0x10, 0xaa, 0x31, 0x12, 0x34};
    const auto seq = parseTransportCcSeq(0xbede, data, sizeof(data), /*transportCcId*/ 3);
    ASSERT_TRUE(seq.has_value());
    EXPECT_EQ(*seq, 0x1234);
}

TEST(Twcc, parseReturnsNulloptWhenAbsent)
{
    const uint8_t data[] = {0x10, 0xaa}; //< Only id=1 present.
    EXPECT_FALSE(parseTransportCcSeq(0xbede, data, sizeof(data), /*transportCcId*/ 3));
}

TEST(Twcc, buildTwoPacketsGoldenBytes)
{
    TwccFeedbackBuilder builder;
    builder.onPacket(/*seq*/ 100, microseconds(64'000));
    builder.onPacket(/*seq*/ 101, microseconds(64'500));

    uint8_t dst[64] = {};
    const int written = builder.build(
        dst, sizeof(dst), /*senderSsrc*/ 0x01020304, /*mediaSourceSsrc*/ 0x0a0b0c0d);

    const std::vector<uint8_t> expected = {
        0x8f, 0xcd, // V=2, P=0, FMT=15 | PT=205 (RTPFB).
        0x00, 0x05, // Length in 32-bit words minus 1 = 24/4 - 1.
        0x01, 0x02, 0x03, 0x04, // Sender SSRC.
        0x0a, 0x0b, 0x0c, 0x0d, // Media source SSRC.
        0x00, 0x64, // Base sequence = 100.
        0x00, 0x02, // Packet status count = 2.
        0x00, 0x00, 0x01, // Reference time = 64000us / 64ms = 1.
        0x00, // Feedback packet count.
        0x20, 0x02, // Run-length chunk: symbol=small(1), run=2.
        0x00, 0x02, // Recv deltas: 0 and 2 ticks (0us, 500us).
    };
    ASSERT_EQ(written, (int) expected.size());
    EXPECT_EQ(std::vector<uint8_t>(dst, dst + written), expected);
}

TEST(Twcc, buildSinglePacketSetsPaddingBit)
{
    TwccFeedbackBuilder builder;
    builder.onPacket(/*seq*/ 100, microseconds(64'000));

    uint8_t dst[64] = {};
    const int written = builder.build(
        dst, sizeof(dst), /*senderSsrc*/ 0x01020304, /*mediaSourceSsrc*/ 0x0a0b0c0d);

    const std::vector<uint8_t> expected = {
        0xaf, 0xcd, // Padding bit (0x20) set because the delta section is odd.
        0x00, 0x05, // Length = 24/4 - 1.
        0x01, 0x02, 0x03, 0x04,
        0x0a, 0x0b, 0x0c, 0x0d,
        0x00, 0x64, // Base sequence = 100.
        0x00, 0x01, // Packet status count = 1.
        0x00, 0x00, 0x01,
        0x00,
        0x20, 0x01, // Chunk: symbol=small(1), run=1.
        0x00, // One recv delta = 0 ticks.
        0x01, // Padding: one byte, last byte holds the padding length.
    };
    ASSERT_EQ(written, (int) expected.size());
    EXPECT_EQ(std::vector<uint8_t>(dst, dst + written), expected);
}

TEST(Twcc, feedbackCountIncrementsPerPacket)
{
    TwccFeedbackBuilder builder;
    uint8_t dst[64] = {};

    builder.onPacket(100, microseconds(64'000));
    builder.build(dst, sizeof(dst), 1, 2);
    EXPECT_EQ(dst[19], 0);

    builder.onPacket(101, microseconds(64'500));
    builder.build(dst, sizeof(dst), 1, 2);
    EXPECT_EQ(dst[19], 1);
}

} // namespace nx::webrtc::test
