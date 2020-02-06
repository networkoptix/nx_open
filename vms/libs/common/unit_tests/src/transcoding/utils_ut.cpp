#include <gtest/gtest.h>
#include <transcoding/transcoding_utils.h>


TEST(TranscodingUtils, findEncoderCodecIdTest)
{
    av_register_all();
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
