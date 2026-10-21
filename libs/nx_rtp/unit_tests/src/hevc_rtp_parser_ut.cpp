// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <cstring>
#include <vector>

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

// clang-format off

// Parameter sets and an IDR slice header of a 320x240 stream produced by libx265.
const std::vector<uint8_t> kVps = {
    0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9e, 0x08, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x3c, 0x95, 0x98, 0x09};

const std::vector<uint8_t> kSps = {
    0x42, 0x01, 0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9e, 0x08, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x3c, 0x90, 0x01, 0x41, 0x01, 0xe2, 0xcb, 0x2b, 0x34, 0x92, 0x65, 0x78, 0x0b, 0x70,
    0x20, 0x20, 0x00, 0x40, 0x00, 0x00, 0x03, 0x00, 0x40, 0x00, 0x00, 0x03, 0x00, 0x42};

const std::vector<uint8_t> kPps = {0x44, 0x01, 0xc1, 0x72, 0x86, 0x0c, 0x46, 0x24};

// NAL unit type 20 (IDR_N_LP), first_slice_segment_in_pic_flag is set in the third byte.
const std::vector<uint8_t> kIdr = {0x28, 0x01, 0xaf, 0x13, 0x20, 0xaf};

// clang-format on

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

TEST(HevcRtpParser, PendingChunksSurviveRtpBufferReuse)
{
    // A parser of another payload type of the same track may report data, and the stream provider
    // then clears the RTP buffer the pending chunks of this parser point into. See VMS-63322.
    nx::rtp::HevcParser parser;
    std::vector<uint8_t> rtpBuffer(1024, 0);
    int bufferSize = 0;
    bool gotData = false;

    const auto processPacket = [&](const std::vector<uint8_t>& nalUnit, bool marker)
    {
        memcpy(rtpBuffer.data() + bufferSize, nalUnit.data(), nalUnit.size());
        const int offset = bufferSize;
        bufferSize += (int) nalUnit.size();
        return parser.processData(makeHeader(/*timestamp*/ 1, marker),
            rtpBuffer.data(),
            offset,
            (int) nalUnit.size(),
            gotData);
    };

    ASSERT_TRUE(processPacket(kVps, /*marker*/ false).success);
    ASSERT_TRUE(processPacket(kSps, /*marker*/ false).success);
    ASSERT_TRUE(processPacket(kPps, /*marker*/ false).success);
    ASSERT_FALSE(gotData);

    // The stream provider backs the pending chunks up before it clears the buffer, which is then
    // refilled from offset zero. Backing up twice must be harmless.
    parser.backupPendingData(rtpBuffer.data());
    parser.backupPendingData(rtpBuffer.data());
    std::fill(rtpBuffer.begin(), rtpBuffer.end(), 0xff);
    bufferSize = 0;

    ASSERT_TRUE(processPacket(kIdr, /*marker*/ true).success);
    ASSERT_TRUE(gotData);

    const auto frame = parser.nextData();
    ASSERT_TRUE(frame);
    ASSERT_TRUE(frame->context); //< Built from the parameter sets that have been backed up.

    std::vector<uint8_t> expected;
    for (const auto& nalUnit: {kVps, kSps, kPps, kIdr})
    {
        const uint32_t size = htonl((uint32_t) nalUnit.size());
        const auto sizeBytes = (const uint8_t*) &size;
        expected.insert(expected.end(), sizeBytes, sizeBytes + sizeof(size));
        expected.insert(expected.end(), nalUnit.begin(), nalUnit.end());
    }
    ASSERT_EQ(expected.size(), (size_t) frame->dataSize());
    ASSERT_EQ(0, memcmp(expected.data(), frame->data(), expected.size()));
}

TEST(HevcRtpParser, DropsFrameWithoutCodecParameters)
{
    nx::rtp::HevcParser parser;
    std::vector<uint8_t> rtpBuffer(1024, 0);
    bool gotData = false;

    // A key frame without parameter sets: codec parameters can not be built from it.
    memcpy(rtpBuffer.data(), kIdr.data(), kIdr.size());
    const auto result = parser.processData(makeHeader(/*timestamp*/ 1, /*marker*/ true),
        rtpBuffer.data(),
        /*bufferOffset*/ 0,
        (int) kIdr.size(),
        gotData);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(gotData);
    ASSERT_FALSE(parser.nextData());
}
