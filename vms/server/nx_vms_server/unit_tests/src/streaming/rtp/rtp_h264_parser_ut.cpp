#include <gtest/gtest.h>

#include <nx/streaming/rtp/parsers/h264_rtp_parser.h>

TEST (H264RtpParser, ZeroPacketSize)
{
    uint8_t rtpData[] = {
        0x80, 0xe2, 0x06, 0xb2, 0xab, 0x20, 0xbb, 0x29, 0x19, 0x6d, 0xf5, 0x28,  // RTP header, marker = 1
        0x7c, 0x85, // H264 FU_A + IDR pictrure NAL unit type(without nal unit data)
    };

    nx::streaming::rtp::H264Parser parser;
    bool gotData;
    auto result = parser.processData(rtpData, /*offset*/ 0, /*bytesRead*/ 14, gotData);
    ASSERT_EQ(result.success, true);
}
