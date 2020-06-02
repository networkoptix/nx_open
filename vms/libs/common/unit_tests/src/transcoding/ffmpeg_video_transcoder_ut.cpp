#include <gtest/gtest.h>
#include <transcoding/ffmpeg_video_transcoder.h>

#include <nx/streaming/abstract_stream_data_provider.h>
#include <core/resource/resource.h>
#include <core/resource/param.h>
#include <core/resource/camera_media_stream_info.h>

class MockDataProvider: public QnAbstractStreamDataProvider
{
public:
    MockDataProvider(const QnResourcePtr& resource):
        QnAbstractStreamDataProvider(resource)
    {}
};

std::unique_ptr<QnAbstractStreamDataProvider> getProvider(const QSize& maxResolution)
{
    QnResourcePtr resource(new QnResource());
    resource->setForceUseLocalProperties(true);
    const CameraMediaStreams mediaStreams {{
        {nx::vms::api::StreamIndex::primary, maxResolution},
        {nx::vms::api::StreamIndex::secondary, {640, 480}}
    }};
    resource->setProperty(
        ResourcePropertyKey::kMediaStreams, QString::fromUtf8(QJson::serialized(mediaStreams)));
    return std::make_unique<QnAbstractStreamDataProvider>(resource);
}

QnWritableCompressedVideoDataPtr getVideoData(QnAbstractStreamDataProvider* provider)
{
    QnWritableCompressedVideoDataPtr videoData(
        new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, 1024));
    videoData->compressionType = AV_CODEC_ID_H264;
    videoData->dataProvider = provider;
    return videoData;
}

TEST(FfmpegVideoTranscoder, ResolutionTest)
{
    av_register_all();
    {
        // Use max resolution from resource.
        QSize maxResourceResolution = QSize(1920, 1080);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H263P);
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), maxResourceResolution);
    }
    {
        // Stream 4k as is.
        QSize maxResourceResolution = QSize(4096, 2160);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H264);
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), maxResourceResolution);
    }
    {
        // Stream 4k downscaled due to codec restrictions.
        QSize maxResourceResolution = QSize(4096, 2160);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H263P);
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(2048, 1080));
    }
    {
        // Force source resolution.
        QSize maxResourceResolution = QSize(4096, 2160);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H263P);
        transcoder.setSourceResolution(QSize(640, 480));
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(640, 480));
    }
    {
        // Use target resolution.
        QSize maxResourceResolution = QSize(1920, 1080);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H263P);
        transcoder.setOutputResolutionLimit(QSize(0, 720));
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(1280, 720));
    }
    {
        // Use target resolution and codec resctriction.
        QSize maxResourceResolution = QSize(3820, 2160);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H263P);
        transcoder.setOutputResolutionLimit(QSize(0, 1200));
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(2048, 1152));
    }
    {
        // Use rotation and codec resctriction after rotation.
        QSize maxResourceResolution = QSize(3820, 2160);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H263P);
        nx::core::transcoding::Settings settings;
        settings.rotation = 90;
        nx::core::transcoding::FilterChain filters(settings, QnMediaDewarpingParams(), nullptr);
        transcoder.setFilterChain(filters);
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(652, 1152));
    }
    {
        // Resolution rounding. //TODO fix filter chain to correctly round resoluiton up(#639x480)
        QSize maxResourceResolution = QSize(1280, 960);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H263P);
        transcoder.setOutputResolutionLimit(QSize(641, 480));
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(640, 480));
    }
}
