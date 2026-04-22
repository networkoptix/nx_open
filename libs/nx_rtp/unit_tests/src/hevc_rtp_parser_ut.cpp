// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/rtp/parsers/hevc_rtp_parser.h>

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

TEST(HevcRtpParser, RejectsEmptyPayload)
{
    nx::rtp::HevcParser parser;
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

TEST(HevcRtpParser, RejectsPaciPacket)
{
    nx::rtp::HevcParser parser;
    bool gotData = false;

    // NAL unit type 50 (PACI packet), unsupported by parser.
    uint8_t payload[] = {0x64, 0x01};
    auto result = parser.processData(
        makeHeader(/*timestamp*/ 10),
        payload,
        /*bufferOffset*/ 0,
        sizeof(payload),
        gotData);

    ASSERT_FALSE(result.success);
    ASSERT_FALSE(gotData);
    ASSERT_FALSE(parser.nextData());
}

TEST(HevcRtpParser, RejectsFragmentationPacketWithBothStartAndEndFlags)
{
    nx::rtp::HevcParser parser;
    bool gotData = false;

    uint8_t payload[] = {
        0x62, 0x01, // NAL unit type 49 (fragmentation packet).
        0xD3, // Start and end flags are both set for unit type 19.
        0x80,
    };

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 11),
        payload,
        /*bufferOffset*/ 0,
        sizeof(payload),
        gotData);

    ASSERT_FALSE(result.success);
    ASSERT_FALSE(gotData);
    ASSERT_FALSE(parser.nextData());
}
