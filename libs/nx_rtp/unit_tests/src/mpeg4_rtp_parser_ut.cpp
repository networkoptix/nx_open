// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cstring>

#include <gtest/gtest.h>

#include <nx/rtp/parsers/mpeg4_rtp_parser.h>

namespace {

nx::rtp::RtpHeader makeHeader(uint32_t timestamp, bool marker)
{
    nx::rtp::RtpHeader header{};
    header.version = nx::rtp::RtpHeader::kVersion;
    header.marker = marker;
    header.timestamp = htonl(timestamp);
    return header;
}

} // namespace

TEST(Mpeg4RtpParser, BuildsFrameFromMultiplePackets)
{
    nx::rtp::Mpeg4Parser parser;
    bool gotData = false;

    uint8_t firstPacket[] = {0x00, 0x00, 0x01, 0xB0}; //< Key-frame start code.
    auto result = parser.processData(
        makeHeader(/*timestamp*/ 77, /*marker*/ false),
        firstPacket,
        /*bufferOffset*/ 0,
        sizeof(firstPacket),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(gotData);

    uint8_t secondPacket[] = {0x12, 0x34};
    result = parser.processData(
        makeHeader(/*timestamp*/ 77, /*marker*/ true),
        secondPacket,
        /*bufferOffset*/ 0,
        sizeof(secondPacket),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto frame = parser.nextData();
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->compressionType, AV_CODEC_ID_MPEG4);
    ASSERT_EQ(frame->timestamp, 77);
    ASSERT_TRUE(frame->flags & QnAbstractMediaData::MediaFlags_AVKey);
    ASSERT_EQ(frame->dataSize(), sizeof(firstPacket) + sizeof(secondPacket));

    uint8_t expected[] = {0x00, 0x00, 0x01, 0xB0, 0x12, 0x34};
    ASSERT_EQ(std::memcmp(frame->data(), expected, sizeof(expected)), 0);
}

TEST(Mpeg4RtpParser, ClearDropsIncompleteFrame)
{
    nx::rtp::Mpeg4Parser parser;
    bool gotData = false;

    uint8_t firstPacket[] = {0x01, 0x02, 0x03};
    auto result = parser.processData(
        makeHeader(/*timestamp*/ 1, /*marker*/ false),
        firstPacket,
        /*bufferOffset*/ 0,
        sizeof(firstPacket),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(gotData);

    parser.clear();

    uint8_t secondPacket[] = {0x0A, 0x0B};
    result = parser.processData(
        makeHeader(/*timestamp*/ 1, /*marker*/ true),
        secondPacket,
        /*bufferOffset*/ 0,
        sizeof(secondPacket),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto frame = parser.nextData();
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->dataSize(), sizeof(secondPacket));
    ASSERT_EQ(std::memcmp(frame->data(), secondPacket, sizeof(secondPacket)), 0);
}

TEST(Mpeg4RtpParser, SinglePacketWithoutKeyFrameStartCodeIsNotKeyFrame)
{
    nx::rtp::Mpeg4Parser parser;
    bool gotData = false;

    uint8_t packet[] = {0x00, 0x00, 0x01, 0xB6, 0x55};
    auto result = parser.processData(
        makeHeader(/*timestamp*/ 88, /*marker*/ true),
        packet,
        /*bufferOffset*/ 0,
        sizeof(packet),
        gotData);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto frame = parser.nextData();
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->compressionType, AV_CODEC_ID_MPEG4);
    ASSERT_EQ(frame->timestamp, 88);
    ASSERT_EQ(frame->flags & QnAbstractMediaData::MediaFlags_AVKey, 0);
    ASSERT_EQ(frame->dataSize(), sizeof(packet));
    ASSERT_EQ(std::memcmp(frame->data(), packet, sizeof(packet)), 0);
}
