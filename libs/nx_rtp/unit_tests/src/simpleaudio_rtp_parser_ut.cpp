// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cstring>

#include <gtest/gtest.h>

#include <nx/rtp/parsers/simpleaudio_rtp_parser.h>

namespace {

nx::rtp::RtpHeader makeHeader(uint32_t timestamp)
{
    nx::rtp::RtpHeader header{};
    header.version = nx::rtp::RtpHeader::kVersion;
    header.timestamp = htonl(timestamp);
    return header;
}

} // namespace

TEST(SimpleAudioRtpParser, ParsesSinglePacket)
{
    nx::rtp::SimpleAudioParser parser(AV_CODEC_ID_PCM_MULAW);
    nx::rtp::Sdp::Media sdp;
    sdp.rtpmap.clockRate = 16000;
    sdp.rtpmap.channels = 2;
    parser.setSdpInfo(sdp);

    uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    bool gotData = false;

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 777),
        payload,
        /*bufferOffset*/ 0,
        sizeof(payload),
        gotData);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto mediaData = parser.nextData();
    ASSERT_TRUE(mediaData);
    ASSERT_EQ(mediaData->compressionType, AV_CODEC_ID_PCM_MULAW);
    ASSERT_EQ(mediaData->timestamp, 777);
    ASSERT_EQ(mediaData->dataSize(), sizeof(payload));
    ASSERT_EQ(std::memcmp(mediaData->data(), payload, sizeof(payload)), 0);
    ASSERT_TRUE(mediaData->context);

    const auto codecParameters = parser.getCodecParameters();
    ASSERT_TRUE(codecParameters);
    ASSERT_EQ(codecParameters->getCodecId(), AV_CODEC_ID_PCM_MULAW);
    ASSERT_EQ(codecParameters->getAvCodecParameters()->sample_rate, 16000);
    ASSERT_FALSE(parser.nextData());
}

TEST(SimpleAudioRtpParser, EmptyPayloadIsMalformed)
{
    nx::rtp::SimpleAudioParser parser(AV_CODEC_ID_PCM_MULAW);
    bool gotData = false;
    uint8_t payload = 0;

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 100),
        &payload,
        /*bufferOffset*/ 0,
        /*bytesRead*/ 0,
        gotData);

    ASSERT_FALSE(result.success);
    ASSERT_FALSE(gotData);
    ASSERT_FALSE(parser.nextData());
}

TEST(SimpleAudioRtpParser, CustomBitsPerSampleUpdatesCodecParameters)
{
    nx::rtp::SimpleAudioParser parser(AV_CODEC_ID_PCM_MULAW);
    parser.setBitsPerSample(16);

    nx::rtp::Sdp::Media sdp;
    sdp.rtpmap.clockRate = 8000;
    sdp.rtpmap.channels = 1;
    parser.setSdpInfo(sdp);

    const auto codecParameters = parser.getCodecParameters();
    ASSERT_TRUE(codecParameters);
    ASSERT_EQ(codecParameters->getCodecId(), AV_CODEC_ID_PCM_MULAW);
    ASSERT_EQ(codecParameters->getChannels(), 1);
    ASSERT_EQ(codecParameters->getSampleRate(), 8000);
    ASSERT_EQ(codecParameters->getBitsPerCodedSample(), 16);
    ASSERT_EQ(codecParameters->getBitRate(), 16 * 8000);
}
