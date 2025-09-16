// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/camera_media_stream_info.h>
#include <core/resource/resource.h>
#include <core/resource/resource_property_key.h>
#include <nx/fusion/model_functions.h>
#include <nx/media/ffmpeg/demuxer.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <transcoding/ffmpeg_video_transcoder.h>

QnWritableCompressedVideoDataPtr getVideoData()
{
    QnWritableCompressedVideoDataPtr videoData(new QnWritableCompressedVideoData(1024));
    videoData->compressionType = AV_CODEC_ID_H264;
    return videoData;
}

TEST(FfmpegVideoTranscoder, ResolutionTest)
{
    {
        // Round target resolution if there is no filters
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H264;
        config.sourceResolution = QSize(720, 570);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(720, 572));
    }

    {
        // Use max resolution from resource.
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H263P;
        config.sourceResolution = QSize(1920, 1080);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(1920, 1080));
    }
    {
        // Stream 4k as is.
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H264;
        config.sourceResolution = QSize(4096, 2160);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(4096, 2160));
    }
    {
        // Stream 4k downscaled due to codec restrictions.
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H263P;
        config.sourceResolution = QSize(4096, 2160);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(2048, 1080));
    }
    {
        // Use target resolution.
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H263P;
        config.sourceResolution = QSize(1920, 1080);
        config.outputResolutionLimit = QSize(0, 720);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(1280, 720));
    }
    {
        // Use target resolution and codec restriction.
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H263P;
        config.sourceResolution = QSize(3820, 2160);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        config.outputResolutionLimit = QSize(0, 1200);
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(2048, 1152));
    }
    {
        // Rotation
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H264;
        config.sourceResolution = QSize(4096, 2160);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        nx::core::transcoding::Settings settings;
        settings.rotation = 90;
        auto filters = std::make_unique<nx::core::transcoding::FilterChain>(
            settings, nx::vms::api::dewarping::MediaData(), nullptr);
        transcoder.setFilterChain(std::move(filters));
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(2160, 4096));
    }
    {
        // Use rotation and codec restriction after rotation.
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H263P;
        config.sourceResolution = QSize(3820, 2160);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        nx::core::transcoding::Settings settings;
        settings.rotation = 90;
        auto filters = std::make_unique<nx::core::transcoding::FilterChain>(
            settings, nx::vms::api::dewarping::MediaData(), nullptr);
        transcoder.setFilterChain(std::move(filters));
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(652, 1152));
    }
    {
        // Resolution rounding down.
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H263P;
        config.sourceResolution = QSize(1280, 960);
        config.outputResolutionLimit = QSize(641, 480);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(640, 480));
    }
    {
        // Resolution rounding up and down.
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H263P;
        config.sourceResolution = QSize(1280, 960);
        config.outputResolutionLimit = QSize(639, 481);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(640, 480));
    }
    {
        // Target should not upscale
        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H264;
        config.sourceResolution = QSize(640, 480);
        config.outputResolutionLimit = QSize(1280, 960);
        QnFfmpegVideoTranscoder transcoder(config, nullptr);
        ASSERT_TRUE(transcoder.open(getVideoData()));
        ASSERT_EQ(transcoder.getOutputResolution(), QSize(640, 480));
    }
}

std::unique_ptr<QnFfmpegVideoTranscoder> createTranscoder(
    const QnConstCompressedVideoDataPtr& videoData)
{
    QnFfmpegVideoTranscoder::Config config;
    config.targetCodecId = AV_CODEC_ID_H264;
    config.useMultiThreadEncode = true;
    config.decoderConfig.mtDecodePolicy = MultiThreadDecodePolicy::enabled;
    config.quality = Qn::StreamQuality::highest;
    auto transcoder = std::make_unique<QnFfmpegVideoTranscoder>(config, nullptr);

    if (!transcoder->open(videoData))
        return nullptr;

    return transcoder;
}

TEST(FfmpegVideoTranscoder, FlushBFrames)
{
    QFile mkvFile(":/test_b_frames.mkv");
    ASSERT_TRUE(mkvFile.open(QIODevice::ReadOnly));
    QByteArray mkvByteArray = mkvFile.readAll();

    auto ioInput = nx::media::ffmpeg::fromBuffer(
        (const uint8_t*)mkvByteArray.data(), mkvByteArray.size());

    ASSERT_TRUE(ioInput);
    nx::media::ffmpeg::Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(std::move(ioInput)));

    int inputFrames = 0;
    int outFrames = 0;
    std::unique_ptr<QnFfmpegVideoTranscoder> transcoder;
    while(true)
    {
        auto videoData = demuxer.getNextData();
        if (!videoData)
            break;

        if (!transcoder)
            transcoder = createTranscoder(std::dynamic_pointer_cast<QnCompressedVideoData>(videoData));
        ASSERT_TRUE(transcoder != nullptr);

        QnAbstractMediaDataPtr result;
        ++inputFrames;
        transcoder->transcodePacket(videoData, &result);
        if (result && result->dataSize() > 0)
            ++outFrames;
    }
    // Flush
    while (true)
    {
        QnAbstractMediaDataPtr result;
        transcoder->transcodePacket(QnAbstractMediaDataPtr(), &result);
        if (!result || result->dataSize() == 0)
            break;

        NX_DEBUG(this, "flushed packet: %1", result->timestamp);
        ++outFrames;
    }
    ASSERT_EQ(outFrames, inputFrames);
}
