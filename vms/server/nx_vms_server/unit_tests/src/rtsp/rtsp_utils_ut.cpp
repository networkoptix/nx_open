
#include <gtest/gtest.h>
#include <rtsp/rtsp_utils.h>

extern "C"
{
    #include <libavformat/avformat.h>
}

TEST(RtspUtils, findEncoderCodecIdTest)
{
    av_register_all();
    ASSERT_EQ(nx::rtsp::findEncoderCodecId("mjpeg"), AV_CODEC_ID_MJPEG);
    ASSERT_EQ(nx::rtsp::findEncoderCodecId("h263p"), AV_CODEC_ID_H263P);
    ASSERT_EQ(nx::rtsp::findEncoderCodecId("H263p"), AV_CODEC_ID_H263P);
    ASSERT_EQ(nx::rtsp::findEncoderCodecId("libvpx"), AV_CODEC_ID_VP8);
    ASSERT_EQ(nx::rtsp::findEncoderCodecId("mpeg4"), AV_CODEC_ID_MPEG4);
    ASSERT_EQ(nx::rtsp::findEncoderCodecId("MPEG4"), AV_CODEC_ID_MPEG4);
    ASSERT_EQ(nx::rtsp::findEncoderCodecId("mpeg2video"), AV_CODEC_ID_MPEG2VIDEO);
}
