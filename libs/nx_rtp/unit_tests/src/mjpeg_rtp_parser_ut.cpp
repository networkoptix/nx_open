// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include <nx/media/video_data_packet.h>
#include <nx/rtp/parsers/mjpeg_rtp_parser.h>

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

TEST(MjpegRtpParser, RejectsTooSmallPacket)
{
    nx::rtp::MjpegParser parser;
    bool gotData = false;
    uint8_t payload[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 1, /*marker*/ false),
        payload,
        /*bufferOffset*/ 0,
        sizeof(payload),
        gotData);

    ASSERT_FALSE(result.success);
    ASSERT_FALSE(gotData);
}

TEST(MjpegRtpParser, BuildsFrameForSinglePacket)
{
    nx::rtp::MjpegParser parser;
    bool gotData = false;

    // RFC2435 main JPEG header:
    // type-specific(1), fragment offset(3), type(1), q(1), width(1), height(1).
    uint8_t payload[] = {
        0x00, // type-specific.
        0x00, 0x00, 0x00, // fragment offset.
        0x00, // type.
        0x32, // q.
        0x01, // width in 8-pixel blocks -> 8px.
        0x01, // height in 8-pixel blocks -> 8px.
    };

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 500, /*marker*/ true),
        payload,
        /*bufferOffset*/ 0,
        sizeof(payload),
        gotData);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto frame = parser.nextData();
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->compressionType, AV_CODEC_ID_MJPEG);
    ASSERT_EQ(frame->timestamp, 500);
    ASSERT_TRUE(frame->flags & QnAbstractMediaData::MediaFlags_AVKey);
    ASSERT_GE(frame->dataSize(), 4u);
    ASSERT_EQ((uint8_t) frame->data()[0], 0xff);
    ASSERT_EQ((uint8_t) frame->data()[1], 0xd8);
    ASSERT_EQ((uint8_t) frame->data()[frame->dataSize() - 2], 0xff);
    ASSERT_EQ((uint8_t) frame->data()[frame->dataSize() - 1], 0xd9);
}

TEST(MjpegRtpParser, UsesConfiguredResolutionToRestoreWrappedDimensions)
{
    nx::rtp::MjpegParser parser;
    parser.setConfiguredResolution(/*width*/ 3840, /*height*/ 2160);

    bool gotData = false;
    uint8_t payload[] = {
        0x00, // type-specific.
        0x00, 0x00, 0x00, // fragment offset.
        0x00, // type.
        0x32, // q.
        0xE0, // width in 8-pixel blocks after 2048 wrap.
        0x0E, // height in 8-pixel blocks after 2048 wrap.
    };

    auto result = parser.processData(
        makeHeader(/*timestamp*/ 900, /*marker*/ true),
        payload,
        /*bufferOffset*/ 0,
        sizeof(payload),
        gotData);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto frame = parser.nextData();
    ASSERT_TRUE(frame);
    auto videoFrame = std::dynamic_pointer_cast<QnCompressedVideoData>(frame);
    ASSERT_TRUE(videoFrame);
    ASSERT_EQ(videoFrame->compressionType, AV_CODEC_ID_MJPEG);
    ASSERT_EQ(videoFrame->timestamp, 900);
    ASSERT_EQ(videoFrame->width, 3840);
    ASSERT_EQ(videoFrame->height, 2160);
}

TEST(MjpegRtpParser, PendingChunksSurviveRtpBufferReuse)
{
    // A parser of another payload type of the same track may report data, and the stream provider
    // then clears the RTP buffer the pending chunks of this parser point into. See VMS-63322.
    nx::rtp::MjpegParser parser;
    bool gotData = false;
    std::vector<uint8_t> rtpBuffer(1024, 0);

    // clang-format off
    const uint8_t firstPacket[] = {
        0x00, // type-specific.
        0x00, 0x00, 0x00, // fragment offset.
        0x00, // type.
        0x32, // q.
        0x04, // width in 8-pixel blocks -> 32px.
        0x04, // height in 8-pixel blocks -> 32px.
        0xaa, 0xbb, 0xcc, 0xdd, // Scan data.
    };
    const uint8_t secondPacket[] = {
        0x00, 0x00, 0x00, 0x04, 0x00, 0x32, 0x04, 0x04, //< Non zero fragment offset.
        0x11, 0x22, 0xff, 0xd9, //< Scan data with the end of image marker.
    };
    // clang-format on

    memcpy(rtpBuffer.data(), firstPacket, sizeof(firstPacket));
    auto result = parser.processData(makeHeader(/*timestamp*/ 7, /*marker*/ false),
        rtpBuffer.data(),
        /*bufferOffset*/ 0,
        sizeof(firstPacket),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(gotData);

    // The stream provider backs the pending chunks up before it clears the buffer, which is then
    // refilled from offset zero.
    parser.backupPendingData(rtpBuffer.data());
    std::fill(rtpBuffer.begin(), rtpBuffer.end(), 0xee);

    memcpy(rtpBuffer.data(), secondPacket, sizeof(secondPacket));
    result = parser.processData(makeHeader(/*timestamp*/ 7, /*marker*/ true),
        rtpBuffer.data(),
        /*bufferOffset*/ 0,
        sizeof(secondPacket),
        gotData);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);

    auto frame = parser.nextData();
    ASSERT_TRUE(frame);
    const uint8_t expectedScanData[] = {0xaa, 0xbb, 0xcc, 0xdd, 0x11, 0x22, 0xff, 0xd9};
    ASSERT_GE(frame->dataSize(), sizeof(expectedScanData));
    ASSERT_EQ(0,
        memcmp(expectedScanData,
            frame->data() + frame->dataSize() - sizeof(expectedScanData),
            sizeof(expectedScanData)));
}
