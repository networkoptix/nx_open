// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cstring>

#include <gtest/gtest.h>

#include <nx/media/in_stream_compressed_metadata.h>
#include <nx/rtp/parsers/base_metadata_rtp_parser.h>

namespace {

nx::rtp::RtpHeader makeHeader(uint32_t timestamp, bool marker = false)
{
    nx::rtp::RtpHeader header{};
    header.version = nx::rtp::RtpHeader::kVersion;
    header.marker = marker;
    header.timestamp = htonl(timestamp);
    return header;
}

} // namespace

TEST(BaseMetadataRtpParser, MarkerBitFlushesMetadata)
{
    nx::rtp::BaseMetadataRtpParser parser;
    nx::rtp::Sdp::Media sdp;
    sdp.rtpmap.codecName = "vnd.onvif.metadata";
    sdp.rtpmap.clockRate = 90000;
    parser.setSdpInfo(sdp);

    uint8_t payload[] = {0x10, 0x20, 0x30};
    bool gotData = false;
    auto result = parser.processData(
        makeHeader(/*timestamp*/ 1234, /*marker*/ true),
        payload,
        /*bufferOffset*/ 0,
        sizeof(payload),
        gotData);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto mediaData = parser.nextData();
    auto metadata = std::dynamic_pointer_cast<nx::media::InStreamCompressedMetadata>(mediaData);
    ASSERT_TRUE(metadata);
    ASSERT_EQ(metadata->timestamp, 1234);
    ASSERT_EQ(metadata->codec(), sdp.rtpmap.codecName);
    ASSERT_EQ(metadata->dataSize(), sizeof(payload));
    ASSERT_EQ(std::memcmp(metadata->data(), payload, sizeof(payload)), 0);
    ASSERT_FALSE(parser.nextData());
}

TEST(BaseMetadataRtpParser, TimestampChangeFlushesPreviousChunkWithoutMarker)
{
    nx::rtp::BaseMetadataRtpParser parser;
    nx::rtp::Sdp::Media sdp;
    sdp.rtpmap.codecName = "vnd.onvif.metadata";
    parser.setSdpInfo(sdp);

    uint8_t firstChunk[] = {0x01, 0x02};
    uint8_t secondChunk[] = {0x03, 0x04};
    bool gotData = false;

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 100),
        firstChunk,
        /*bufferOffset*/ 0,
        sizeof(firstChunk),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(gotData);

    result = parser.processData(
        makeHeader(/*timestamp*/ 200),
        secondChunk,
        /*bufferOffset*/ 0,
        sizeof(secondChunk),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto mediaData = parser.nextData();
    auto metadata = std::dynamic_pointer_cast<nx::media::InStreamCompressedMetadata>(mediaData);
    ASSERT_TRUE(metadata);
    ASSERT_EQ(metadata->timestamp, 100);
    ASSERT_EQ(metadata->dataSize(), sizeof(firstChunk));
    ASSERT_EQ(std::memcmp(metadata->data(), firstChunk, sizeof(firstChunk)), 0);
}

TEST(BaseMetadataRtpParser, AccumulatesPayloadAcrossPacketsUntilMarker)
{
    nx::rtp::BaseMetadataRtpParser parser;
    nx::rtp::Sdp::Media sdp;
    sdp.rtpmap.codecName = "vnd.onvif.metadata";
    parser.setSdpInfo(sdp);

    uint8_t firstChunk[] = {0x01, 0x02};
    uint8_t secondChunk[] = {0x03, 0x04, 0x05};
    uint8_t expected[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    bool gotData = false;

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 300),
        firstChunk,
        /*bufferOffset*/ 0,
        sizeof(firstChunk),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(gotData);

    result = parser.processData(
        makeHeader(/*timestamp*/ 300, /*marker*/ true),
        secondChunk,
        /*bufferOffset*/ 0,
        sizeof(secondChunk),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto mediaData = parser.nextData();
    auto metadata = std::dynamic_pointer_cast<nx::media::InStreamCompressedMetadata>(mediaData);
    ASSERT_TRUE(metadata);
    ASSERT_EQ(metadata->timestamp, 300);
    ASSERT_EQ(metadata->codec(), sdp.rtpmap.codecName);
    ASSERT_EQ(metadata->dataSize(), sizeof(expected));
    ASSERT_EQ(std::memcmp(metadata->data(), expected, sizeof(expected)), 0);
    ASSERT_FALSE(parser.nextData());
}
