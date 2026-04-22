// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include <nx/rtp/parsers/mpeg12_audio_rtp_parser.h>

namespace {

nx::rtp::RtpHeader makeHeader(uint32_t timestamp)
{
    nx::rtp::RtpHeader header{};
    header.version = nx::rtp::RtpHeader::kVersion;
    header.timestamp = htonl(timestamp);
    return header;
}

std::vector<uint8_t> makeMpegAudioPacket(uint16_t fragmentOffset)
{
    std::vector<uint8_t> packet(40, 0);

    // RFC2250 audio payload header:
    // MBZ(16 bit) + fragment offset(16 bit).
    packet[2] = static_cast<uint8_t>((fragmentOffset >> 8) & 0xff);
    packet[3] = static_cast<uint8_t>(fragmentOffset & 0xff);

    // MPEG1 layer III header (valid for parser).
    packet[4] = 0xFF;
    packet[5] = 0xFB;
    packet[6] = 0x90;
    packet[7] = 0x64;

    for (int i = 8; i < (int) packet.size(); ++i)
        packet[i] = static_cast<uint8_t>(i);

    return packet;
}

std::vector<uint8_t> makeContinuationPacket(
    uint16_t fragmentOffset,
    const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> packet(4, 0);
    packet[2] = static_cast<uint8_t>((fragmentOffset >> 8) & 0xff);
    packet[3] = static_cast<uint8_t>(fragmentOffset & 0xff);
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

} // namespace

TEST(Mpeg12AudioRtpParser, EmitsFrameOnTimestampSwitch)
{
    nx::rtp::Mpeg12AudioParser parser;
    const auto packet = makeMpegAudioPacket(/*fragmentOffset*/ 0);
    bool gotData = false;

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 1000),
        const_cast<uint8_t*>(packet.data()),
        /*bufferOffset*/ 0,
        packet.size(),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(gotData);

    result = parser.processData(
        makeHeader(/*timestamp*/ 2000),
        const_cast<uint8_t*>(packet.data()),
        /*bufferOffset*/ 0,
        packet.size(),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto frame = parser.nextData();
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->timestamp, 1000);
    ASSERT_EQ(frame->compressionType, AV_CODEC_ID_MP3);
    ASSERT_TRUE(frame->context);
    ASSERT_GT(frame->dataSize(), 0U);
}

TEST(Mpeg12AudioRtpParser, RejectsTooSmallPacket)
{
    nx::rtp::Mpeg12AudioParser parser;
    bool gotData = false;
    uint8_t payload[] = {0x00, 0x00, 0x00, 0x00};

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 1),
        payload,
        /*bufferOffset*/ 0,
        sizeof(payload),
        gotData);

    ASSERT_FALSE(result.success);
}

TEST(Mpeg12AudioRtpParser, AppendsContinuationFragmentsBeforeTimestampSwitch)
{
    nx::rtp::Mpeg12AudioParser parser;
    const auto firstPacket = makeMpegAudioPacket(/*fragmentOffset*/ 0);
    const auto continuationPacket =
        makeContinuationPacket(/*fragmentOffset*/ 12, std::vector<uint8_t>{0xAA, 0xBB, 0xCC});
    const auto nextFramePacket = makeMpegAudioPacket(/*fragmentOffset*/ 0);
    bool gotData = false;

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 1000),
        const_cast<uint8_t*>(firstPacket.data()),
        /*bufferOffset*/ 0,
        firstPacket.size(),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(gotData);

    result = parser.processData(
        makeHeader(/*timestamp*/ 1000),
        const_cast<uint8_t*>(continuationPacket.data()),
        /*bufferOffset*/ 0,
        continuationPacket.size(),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(gotData);

    result = parser.processData(
        makeHeader(/*timestamp*/ 2000),
        const_cast<uint8_t*>(nextFramePacket.data()),
        /*bufferOffset*/ 0,
        nextFramePacket.size(),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto frame = parser.nextData();
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->timestamp, 1000);
    ASSERT_EQ(frame->compressionType, AV_CODEC_ID_MP3);
    ASSERT_TRUE(frame->context);
    ASSERT_EQ(frame->context->getSampleRate(), 44100);
    ASSERT_EQ(frame->context->getChannels(), 2);

    const auto firstPayloadSize = firstPacket.size() - 4;
    const auto continuationPayloadSize = continuationPacket.size() - 4;
    ASSERT_EQ(frame->dataSize(), firstPayloadSize + continuationPayloadSize);
    ASSERT_EQ(std::memcmp(frame->data(), firstPacket.data() + 4, firstPayloadSize), 0);
    ASSERT_EQ(
        std::memcmp(
            frame->data() + firstPayloadSize,
            continuationPacket.data() + 4,
            continuationPayloadSize),
        0);
}
