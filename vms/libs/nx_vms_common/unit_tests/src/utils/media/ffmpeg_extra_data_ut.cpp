// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/codec/hevc/hevc_decoder_configuration_record.h>
#include <nx/media/h264_utils.h>

TEST(media, build_extradata_h264)
{
    uint8_t frameData[] = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x29, 0xe3, 0x50, 0x14, 0x07, 0xb6, 0x02, 0xdc,
        0x04, 0x04, 0x06, 0x90, 0x78, 0x91, 0x15, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x3c, 0x80,
        0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00, 0x00, 0x0c, 0x21, 0x18, 0xa0, 0x00, 0x67,
        0xf9, 0x39, 0x39, 0x39, 0x39, 0x38, 0x8f, 0xd1, 0x1e, 0xa4, 0xe2, 0x35, 0xe9, 0x38 };
    const int kSpsSize = 18;
    const int kPpsSize = 4;

    std::vector<uint8_t> extradata = nx::media::h264::buildExtraDataMp4FromAnnexB(
        frameData, sizeof(frameData));

    ASSERT_EQ(extradata.size(), 33);
    ASSERT_EQ(extradata[6], 0); // sps size first byte
    ASSERT_EQ(extradata[7], kSpsSize); // sps size second byte
    ASSERT_EQ(memcmp(extradata.data() + 8, frameData + 4, kSpsSize), 0);
    ASSERT_EQ(extradata[8 + kSpsSize], 1); // pps count
    ASSERT_EQ(extradata[8 + kSpsSize + 1], 0); // pps size first byte
    ASSERT_EQ(extradata[8 + kSpsSize + 2], kPpsSize); // pps size second byte
    ASSERT_EQ(memcmp(extradata.data() + 11 + kSpsSize, frameData + 8 + kSpsSize, kPpsSize), 0);
}

TEST(media, parse_extradata_h265)
{
    uint8_t frameData[] = {
        0x01, 0x01, 0x60, 0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x99, 0xf0, 0x00,
        0xfc, 0xfd, 0xf8, 0xf8, 0x00, 0x00, 0x0f, 0x03, 0x20, 0x00, 0x01, 0x00, 0x17, 0x40, 0x01,
        0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0xb0, 0x00, 0x00, 0x03, 0x00,
        0x00, 0x03, 0x00, 0x99, 0xac, 0x09, 0x21, 0x00, 0x01, 0x00, 0x1e, 0x42, 0x01, 0x01, 0x01,
        0x60, 0x00, 0x00, 0x03, 0x00, 0xb0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x99, 0xa0,
        0x00, 0x80, 0x08, 0x00, 0x87, 0x16, 0x36, 0xb9, 0x24, 0xcb, 0xc4, 0x22, 0x00, 0x01, 0x00,
        0x08, 0x44, 0x01, 0xc0, 0xf2, 0xf0, 0x3c, 0x90, 0x00};

    // Test read.
    nx::media::hevc::HEVCDecoderConfigurationRecord hvcc;
    ASSERT_TRUE(hvcc.read(frameData, sizeof(frameData)));
    ASSERT_EQ(hvcc.sps.size(), 1);
    ASSERT_EQ(hvcc.pps.size(), 1);
    ASSERT_EQ(hvcc.vps.size(), 1);
    ASSERT_EQ(hvcc.sps[0].size(), 30);
    ASSERT_EQ(hvcc.pps[0].size(), 8);
    ASSERT_EQ(hvcc.vps[0].size(), 23);

    // Test write.
    uint8_t serializedData[sizeof(frameData)];
    ASSERT_EQ(hvcc.size(), sizeof(frameData));
    ASSERT_TRUE(hvcc.write(serializedData, sizeof(frameData)));
    ASSERT_EQ(memcmp(serializedData, frameData, sizeof(frameData)), 0);
}
