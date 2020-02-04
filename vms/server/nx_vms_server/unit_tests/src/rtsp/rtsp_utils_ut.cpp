
#include <gtest/gtest.h>
#include <rtsp/rtsp_utils.h>

extern "C"
{
    #include <libavformat/avformat.h>
}

TEST(RtspUtils, parseResolution)
{
    ASSERT_EQ(nx::rtsp::parseResolution("480p"), QSize(0,480));
    ASSERT_EQ(nx::rtsp::parseResolution("640x480"), QSize(640,480));
    ASSERT_FALSE(nx::rtsp::parseResolution("23432432").isValid());
    ASSERT_FALSE(nx::rtsp::parseResolution("werwerwerew").isValid());
}

TEST(RtspUtils, parseUrlParams)
{
    nx::rtsp::UrlParams params;
    ASSERT_TRUE(params.parse(QUrlQuery("codec=h263p&resolution=720p")));
    ASSERT_FALSE(params.quality);
    ASSERT_FALSE(params.position);
    ASSERT_FALSE(params.onvifReplay);
    ASSERT_TRUE(params.codec);
    ASSERT_EQ(params.codec, AV_CODEC_ID_H263P);
    ASSERT_TRUE(params.resolution);
    ASSERT_EQ(params.resolution, QSize(0,720));

    ASSERT_TRUE(params.parse(QUrlQuery("stream=1&onvif_replay")));
    ASSERT_TRUE(params.quality);
    ASSERT_TRUE(params.onvifReplay);
    ASSERT_TRUE(params.onvifReplay.value());
    ASSERT_EQ(params.quality, MEDIA_Quality_Low);

    ASSERT_FALSE(params.parse(QUrlQuery("stream=3")));

    ASSERT_TRUE(params.parse(QUrlQuery("pos=1556297981000")));
    ASSERT_TRUE(params.position);
    ASSERT_EQ(params.position, 1556297981000 * 1000);
}
