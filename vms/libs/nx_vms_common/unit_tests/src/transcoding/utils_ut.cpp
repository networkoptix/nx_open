// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <transcoding/transcoding_utils.h>


TEST(TranscodingUtils, findEncoderCodecIdTest)
{
    ASSERT_EQ(nx::transcoding::findEncoderCodecId("mjpeg"), AV_CODEC_ID_MJPEG);
    ASSERT_EQ(nx::transcoding::findEncoderCodecId("h264"), AV_CODEC_ID_H264);
    ASSERT_EQ(nx::transcoding::findEncoderCodecId("H264"), AV_CODEC_ID_H264);
    ASSERT_EQ(nx::transcoding::findEncoderCodecId("h263"), AV_CODEC_ID_H263P);
    ASSERT_EQ(nx::transcoding::findEncoderCodecId("h263p"), AV_CODEC_ID_H263P);
    ASSERT_EQ(nx::transcoding::findEncoderCodecId("H263p"), AV_CODEC_ID_H263P);
    ASSERT_EQ(nx::transcoding::findEncoderCodecId("libvpx"), AV_CODEC_ID_VP8);
    ASSERT_EQ(nx::transcoding::findEncoderCodecId("mpeg4"), AV_CODEC_ID_MPEG4);
    ASSERT_EQ(nx::transcoding::findEncoderCodecId("MPEG4"), AV_CODEC_ID_MPEG4);
    ASSERT_EQ(nx::transcoding::findEncoderCodecId("mpeg2video"), AV_CODEC_ID_MPEG2VIDEO);
}

TEST(TranscodingUtils, adjustCodecRestrictions)
{
    ASSERT_EQ(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_H263P, QSize(640, 480)), QSize(640, 480));
    ASSERT_EQ(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_H264, QSize(640, 480)), QSize(640, 480));
    ASSERT_EQ(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_H265, QSize(640, 480)), QSize(640, 480));
    ASSERT_EQ(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_MPEG2VIDEO, QSize(640, 480)), QSize(640, 480));
    ASSERT_EQ(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_H264, QSize(2048, 1152)), QSize(2048, 1152));
    ASSERT_EQ(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_H265, QSize(2048, 1152)), QSize(2048, 1152));
    ASSERT_EQ(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_H263P, QSize(2048, 1152)), QSize(2048, 1152));
    ASSERT_EQ(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_MPEG2VIDEO, QSize(2048, 1152)), QSize(2048, 1152));

    // Should be croped to max allowed size
    ASSERT_EQ(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_H263P, QSize(4096, 2304)), QSize(2048, 1152));
    ASSERT_EQ(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_H263P, QSize(4096, 4096)), QSize(1152, 1152));

    // Should avoid 4096 align
    ASSERT_NE(nx::transcoding::adjustCodecRestrictions(AV_CODEC_ID_MPEG2VIDEO, QSize(4096, 2304)), QSize(4096, 2304));
}

TEST(TranscodingUtils, normalizeResolution)
{
    ASSERT_EQ(nx::transcoding::normalizeResolution(QSize(-1, -1), QSize(-1, -1)), QSize(-1, -1));

    // Auto by height.
    ASSERT_EQ(nx::transcoding::normalizeResolution(QSize(0, 1080), QSize(6400, 4800)), QSize(1440, 1080));
    ASSERT_EQ(nx::transcoding::normalizeResolution(QSize(0, 480), QSize(6400, 4800)), QSize(640, 480));

    // Target resolution specified.
    ASSERT_EQ(nx::transcoding::normalizeResolution(QSize(1920, 1080), QSize(640, 480)), QSize(1920, 1080));
}
