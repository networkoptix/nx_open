// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/camera_media_stream_info.h>
#include <core/resource/resource.h>
#include <core/resource/resource_property_key.h>
#include <nx/fusion/model_functions.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <transcoding/ffmpeg_video_transcoder.h>

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
    resource->setForceUsingLocalProperties();
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
    QnWritableCompressedVideoDataPtr videoData(new QnWritableCompressedVideoData(1024));
    videoData->compressionType = AV_CODEC_ID_H264;
    videoData->dataProvider = provider;
    return videoData;
}

TEST(FfmpegVideoTranscoder, ResolutionTest)
{
    {
        // Round target resolution if there is no filters
        QSize sourceResolution = QSize(720, 570);
        auto provider = getProvider(sourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H264);
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(720, 572));
    }

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
        // Rotation
        QSize maxResourceResolution = QSize(4096, 2160);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H264);
        nx::core::transcoding::Settings settings;
        settings.rotation = 90;
        nx::core::transcoding::FilterChain filters(settings, {}, nullptr);
        transcoder.setFilterChain(filters);
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(2160, 4096));
    }
    {
        // Use rotation and codec resctriction after rotation.
        QSize maxResourceResolution = QSize(3820, 2160);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H263P);
        nx::core::transcoding::Settings settings;
        settings.rotation = 90;
        nx::core::transcoding::FilterChain filters(settings, {}, nullptr);
        transcoder.setFilterChain(filters);
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(652, 1152));
    }
    {
        // Resolution rounding down.
        QSize maxResourceResolution = QSize(1280, 960);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H263P);
        transcoder.setOutputResolutionLimit(QSize(641, 480));
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(640, 480));
    }
    {
        // Resolution rounding up and down.
        QSize maxResourceResolution = QSize(1280, 960);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H263P);
        transcoder.setOutputResolutionLimit(QSize(639, 481));
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(640, 480));
    }
    {
        // Target should not upscale
        QSize maxResourceResolution = QSize(640, 480);
        auto provider = getProvider(maxResourceResolution);
        QnFfmpegVideoTranscoder transcoder(DecoderConfig(), nullptr, AV_CODEC_ID_H264);
        transcoder.setOutputResolutionLimit(QSize(1280, 960));
        ASSERT_TRUE(transcoder.open(getVideoData(provider.get())));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(640, 480));
    }
}
