// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cstring>

#include <gtest/gtest.h>

#include <nx/rtp/parsers/aac_rtp_parser.h>

namespace {

nx::rtp::RtpHeader makeHeader(uint32_t timestamp)
{
    nx::rtp::RtpHeader header{};
    header.version = nx::rtp::RtpHeader::kVersion;
    header.timestamp = htonl(timestamp);
    return header;
}

} // namespace

TEST(AacRtpParser, ParsesConstantSizeUnits)
{
    nx::rtp::AacParser parser;
    nx::rtp::Sdp::Media sdp;
    sdp.rtpmap.clockRate = 16000;
    sdp.rtpmap.channels = 1;
    sdp.fmtp.params = {"constantSize=2", "config=1408"};
    parser.setSdpInfo(sdp);

    uint8_t payload[] = {0xaa, 0xbb, 0xcc, 0xdd};
    bool gotData = false;
    auto result = parser.processData(
        makeHeader(/*timestamp*/ 5555),
        payload,
        /*bufferOffset*/ 0,
        sizeof(payload),
        gotData);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto firstUnit = parser.nextData();
    ASSERT_TRUE(firstUnit);
    ASSERT_EQ(firstUnit->compressionType, AV_CODEC_ID_AAC);
    ASSERT_EQ(firstUnit->timestamp, 5555);
    ASSERT_EQ(firstUnit->dataSize(), 2U);
    ASSERT_EQ(std::memcmp(firstUnit->data(), payload, 2), 0);

    auto secondUnit = parser.nextData();
    ASSERT_TRUE(secondUnit);
    ASSERT_EQ(secondUnit->compressionType, AV_CODEC_ID_AAC);
    ASSERT_EQ(secondUnit->timestamp, 5555);
    ASSERT_EQ(secondUnit->dataSize(), 2U);
    ASSERT_EQ(std::memcmp(secondUnit->data(), payload + 2, 2), 0);

    ASSERT_FALSE(parser.nextData());
}

TEST(AacRtpParser, EmptyPacketIsMalformed)
{
    nx::rtp::AacParser parser;
    bool gotData = false;
    uint8_t payload = 0;

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 1),
        &payload,
        /*bufferOffset*/ 0,
        /*bytesRead*/ 0,
        gotData);

    ASSERT_FALSE(result.success);
    ASSERT_FALSE(gotData);
}

TEST(AacRtpParser, CreatesCodecParametersFromSdpConfig)
{
    nx::rtp::AacParser parser;
    nx::rtp::Sdp::Media sdp;
    sdp.rtpmap.clockRate = 16000;
    sdp.rtpmap.channels = 1;
    sdp.fmtp.params = {"constantSize=2", "config=1408"};
    parser.setSdpInfo(sdp);

    const auto codecParameters = parser.getCodecParameters();
    ASSERT_TRUE(codecParameters);
    ASSERT_EQ(codecParameters->getCodecId(), AV_CODEC_ID_AAC);
    ASSERT_EQ(codecParameters->getSampleRate(), 16000);
    ASSERT_EQ(codecParameters->getChannels(), 1);
    ASSERT_EQ(codecParameters->getExtradataSize(), 2);

    const uint8_t expectedExtradata[] = {0x14, 0x08};
    ASSERT_EQ(
        std::memcmp(codecParameters->getExtradata(), expectedExtradata, sizeof(expectedExtradata)),
        0);
}
