// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/rtp/parsers/h264_rtp_parser.h>

TEST (H264RtpParser, ZeroPacketSize)
{
    uint8_t rtpData[] = {
        0x7c, 0x85, // H264 FU_A + IDR pictrure NAL unit type(without nal unit data)
    };

    nx::rtp::RtpHeader header;
    memset(&header, 0, sizeof(header));
    header.version = nx::rtp::RtpHeader::kVersion;
    header.marker = 1;
    nx::rtp::H264Parser parser;
    bool gotData;
    auto result = parser.processData(
        header, rtpData, /*offset*/ 0, /*bytesRead*/ sizeof(rtpData), gotData);
    ASSERT_EQ(result.success, true);
}
