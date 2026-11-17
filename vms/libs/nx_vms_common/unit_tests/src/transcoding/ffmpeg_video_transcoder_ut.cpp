// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>

#include <gtest/gtest.h>

extern "C" {
#include <libavutil/hwcontext.h>
} // extern "C"

#include <core/resource/camera_media_stream_info.h>
#include <core/resource/resource.h>
#include <core/resource/resource_property_key.h>
#include <nx/fusion/model_functions.h>
#include <nx/kit/ini_config.h>
#include <nx/media/ffmpeg/demuxer.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/vms/common/nx_vms_common_ini.h>
#include <transcoding/ffmpeg_video_transcoder.h>
#include <transcoding/video_processor.h>

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
        auto filters = std::make_unique<nx::core::transcoding::FilterChain>(settings);
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
        auto filters = std::make_unique<nx::core::transcoding::FilterChain>(settings);
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

        NX_DEBUG(this, "flushed packet: %1", result);
        ++outFrames;
    }
    ASSERT_EQ(outFrames, inputFrames);
}

namespace {

std::shared_ptr<AVBufferRef> vaapiDevice()
{
    AVBufferRef* device = nullptr;
    if (av_hwdevice_ctx_create(&device, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0) < 0)
        return nullptr;
    return std::shared_ptr<AVBufferRef>(device, [](AVBufferRef* d) { av_buffer_unref(&d); });
}

bool hasVaapi()
{
    static const bool result = (bool) vaapiDevice();
    return result;
}

struct TranscodeResult
{
    int inputFrames = 0;
    int outputFrames = 0;
    QSize outputSize;
    AVPixelFormat encoderFormat = AV_PIX_FMT_NONE;
};

// With twoChannels, channel 0 gets only the first frame and channel 1 the whole stream, so on
// flush channel 0's decoder runs dry while channel 1's still holds frames.
TranscodeResult transcode(const QByteArray& media,
    QnFfmpegVideoTranscoder::Config config,
    const nx::core::transcoding::Settings& settings = {},
    bool twoChannels = false)
{
    TranscodeResult result;
    nx::media::ffmpeg::Demuxer demuxer;
    if (!demuxer.open(nx::media::ffmpeg::fromBuffer((const uint8_t*) media.data(), media.size())))
        return result;

    std::unique_ptr<QnFfmpegVideoTranscoder> transcoder;
    const auto count = [&](const QnAbstractMediaDataPtr& packet)
    {
        if (!packet || packet->dataSize() == 0)
            return false;
        const auto video = std::dynamic_pointer_cast<QnCompressedVideoData>(packet);
        result.outputSize = QSize(video->width, video->height);
        ++result.outputFrames;
        return true;
    };
    const auto feed = [&](const QnConstCompressedVideoDataPtr& video)
    {
        ++result.inputFrames;
        QnAbstractMediaDataPtr packet;
        transcoder->transcodePacket(video, &packet);
        count(packet);
    };

    while (auto data = demuxer.getNextData())
    {
        auto video = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
        if (!video)
            continue;
        if (!transcoder)
        {
            transcoder = std::make_unique<QnFfmpegVideoTranscoder>(config, nullptr);
            transcoder->setFilterChain(
                std::make_unique<nx::core::transcoding::FilterChain>(settings));
            if (!transcoder->open(video))
                return result;
            result.encoderFormat = transcoder->getCodecContext()->pix_fmt;
        }
        if (twoChannels)
        {
            if (result.inputFrames == 0)
                feed(video);
            video.reset(video->clone());
            video->channelNumber = 1;
        }
        feed(video);
    }
    for (bool flushed = true; transcoder && flushed;)
    {
        QnAbstractMediaDataPtr packet;
        transcoder->transcodePacket(QnAbstractMediaDataPtr(), &packet);
        flushed = count(packet);
    }
    return result;
}

QByteArray readFile(const QString& name)
{
    QFile file(name);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

qint64 lastFrameTime(const QByteArray& media)
{
    qint64 result = 0;
    nx::media::ffmpeg::Demuxer demuxer;
    if (demuxer.open(nx::media::ffmpeg::fromBuffer((const uint8_t*) media.data(), media.size())))
    {
        while (auto data = demuxer.getNextData())
            result = std::max(result, data->timestamp);
    }
    return result;
}

// Luma blocks of distinct brightness, 4 across and 3 down, so that a wrong crop offset or
// rotation direction moves them.
CLVideoDecoderOutputPtr blocks(const QSize& size)
{
    CLVideoDecoderOutputPtr frame(new CLVideoDecoderOutput());
    frame->reallocate(size, AV_PIX_FMT_YUV420P);
    for (int y = 0; y < size.height(); ++y)
    {
        for (int x = 0; x < size.width(); ++x)
        {
            frame->data[0][y * frame->linesize[0] + x] =
                16 + x * 4 / size.width() * 50 + y * 3 / size.height() * 15;
        }
    }
    memset(frame->data[1], 128, frame->linesize[1] * size.height() / 2);
    memset(frame->data[2], 128, frame->linesize[2] * size.height() / 2);
    return frame;
}

double meanLumaDifference(const CLVideoDecoderOutputPtr& a, const CLVideoDecoderOutputPtr& b)
{
    int64_t sum = 0;
    for (int y = 0; y < a->height; ++y)
    {
        for (int x = 0; x < a->width; ++x)
        {
            sum +=
                std::abs(a->data[0][y * a->linesize[0] + x] - b->data[0][y * b->linesize[0] + x]);
        }
    }
    return double(sum) / (a->width * a->height);
}

// Process CPU time on POSIX, wall time on Windows, where VAAPI doesn't exist anyway.
std::chrono::milliseconds cpuTime()
{
    return std::chrono::milliseconds(std::clock() * 1000 / CLOCKS_PER_SEC);
}

// A frames context on the device, as each channel's hardware decoder has one of its own.
std::shared_ptr<AVBufferRef> deviceFrames(AVBufferRef* device, const QSize& size)
{
    std::shared_ptr<AVBufferRef> result(
        av_hwframe_ctx_alloc(device), [](AVBufferRef* frames) { av_buffer_unref(&frames); });
    if (!result)
        return nullptr;
    auto context = (AVHWFramesContext*) result->data;
    context->format = AV_PIX_FMT_VAAPI;
    context->sw_format = AV_PIX_FMT_NV12;
    context->width = size.width();
    context->height = size.height();
    return av_hwframe_ctx_init(result.get()) < 0 ? nullptr : result;
}

CLVideoDecoderOutputPtr upload(AVBufferRef* frames, const CLVideoDecoderOutputPtr& frame)
{
    const auto source = frame->scaled(frame->size(), AV_PIX_FMT_NV12);
    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
    if (!source || av_hwframe_get_buffer(frames, result.data(), 0) < 0
        || av_hwframe_transfer_data(result.data(), source.data(), 0) < 0)
    {
        return {};
    }
    return result;
}

} // namespace

TEST(FfmpegVideoTranscoder, Vaapi)
{
    if (!hasVaapi())
        GTEST_SKIP() << "No VAAPI device";

    nx::kit::IniConfig::Tweaks tweaks;
    tweaks.set(&nx::vms::common::ini().enableHwTranscoding, true);

    QnFfmpegVideoTranscoder::Config config;
    config.targetCodecId = AV_CODEC_ID_H264;
    config.outputResolutionLimit = QSize(0, 64);
    const auto result = transcode(readFile(":/test_b_frames.mkv"), config);

    ASSERT_EQ(result.encoderFormat, AV_PIX_FMT_VAAPI);
    ASSERT_GT(result.inputFrames, 0);
    ASSERT_EQ(result.outputFrames, result.inputFrames);
    ASSERT_EQ(result.outputSize, QSize(64, 64));
}

TEST(VideoProcessor, DeviceMatchesSoftware)
{
    const auto device = vaapiDevice();
    if (!device)
        GTEST_SKIP() << "No VAAPI device";

    const QSize sourceSize(640, 360);
    nx::core::transcoding::Settings scale;
    scale.resolution = QSize(320, 180);
    nx::core::transcoding::Settings stretch;
    stretch.resolution = QSize(96, 160);
    std::vector<nx::core::transcoding::Settings> cases{scale, stretch};

    for (const auto& settings: cases)
    {
        SCOPED_TRACE(nx::format("rotation %1, crop %2, resolution %3",
            settings.rotation,
            settings.zoomWindow,
            settings.resolution.value_or(QSize()))
                .toStdString());
        nx::core::transcoding::FilterChain filters(settings);
        filters.prepare(sourceSize);
        const QSize targetSize = filters.apply(sourceSize);

        nx::transcoding::VideoProcessor software(filters, sourceSize, targetSize, nullptr);
        nx::transcoding::VideoProcessor hardware(filters, sourceSize, targetSize, device.get());
        ASSERT_TRUE(hardware.isHardware());

        const auto expected = software.process(blocks(sourceSize));
        const auto onDevice = hardware.process(blocks(sourceSize));
        ASSERT_TRUE(expected && onDevice);
        ASSERT_TRUE(onDevice->hw_frames_ctx);
        const auto actual = onDevice->toSystemMemory();
        ASSERT_TRUE(actual);
        ASSERT_EQ(actual->size(), targetSize);
        ASSERT_EQ(expected->size(), targetSize);
        EXPECT_LT(meanLumaDifference(expected, actual), 3);
    }
}

TEST(VideoProcessor, SoftwareOnlyFiltersStayInSoftware)
{
    const auto device = vaapiDevice();
    if (!device)
        GTEST_SKIP() << "No VAAPI device";

    nx::core::transcoding::Settings settings;
    settings.enhancement.enabled = true;
    nx::core::transcoding::FilterChain filters(settings);
    const QSize sourceSize(640, 360);
    filters.prepare(sourceSize);
    nx::transcoding::VideoProcessor processor(
        filters, sourceSize, filters.apply(sourceSize), device.get());
    ASSERT_FALSE(processor.isHardware());
}

// Alternating channels with frames contexts of their own keep their graphs: a rebuilt graph would
// give its output a new frames context.
TEST(VideoProcessor, ChannelsKeepTheirGraphs)
{
    const auto device = vaapiDevice();
    if (!device)
        GTEST_SKIP() << "No VAAPI device";

    nx::core::transcoding::Settings settings;
    settings.resolution = QSize(320, 180);
    nx::core::transcoding::FilterChain filters(settings);
    const QSize sourceSize(640, 360);
    filters.prepare(sourceSize);
    nx::transcoding::VideoProcessor processor(
        filters, sourceSize, filters.apply(sourceSize), device.get());
    ASSERT_TRUE(processor.isHardware());

    const std::vector channels{
        deviceFrames(device.get(), sourceSize), deviceFrames(device.get(), sourceSize)};
    ASSERT_TRUE(channels[0] && channels[1]);

    // Kept alive, so that a freed frames context can't come back at the same address.
    std::vector<CLVideoDecoderOutputPtr> outputs;
    for (int i = 0; i < 2; ++i)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto input = upload(channels[channel].get(), blocks(sourceSize));
            ASSERT_TRUE(input);
            input->channel = channel;
            const auto output = processor.process(input);
            ASSERT_TRUE(output && output->hw_frames_ctx);
            outputs.push_back(output);
        }
    }
    EXPECT_EQ(outputs[0]->hw_frames_ctx->data, outputs[2]->hw_frames_ctx->data);
    EXPECT_EQ(outputs[1]->hw_frames_ctx->data, outputs[3]->hw_frames_ctx->data);
}

// Software filters feed the hardware encoder through an upload.
TEST(FfmpegVideoTranscoder, VaapiEncodesAfterSoftwareFilters)
{
    if (!hasVaapi())
        GTEST_SKIP() << "No VAAPI device";

    nx::kit::IniConfig::Tweaks tweaks;
    tweaks.set(&nx::vms::common::ini().enableHwTranscoding, true);

    QnFfmpegVideoTranscoder::Config config;
    config.targetCodecId = AV_CODEC_ID_H264;
    config.outputResolutionLimit = QSize(0, 64);
    nx::core::transcoding::Settings settings;
    settings.enhancement.enabled = true;
    const auto result = transcode(readFile(":/test_b_frames.mkv"), config, settings);

    ASSERT_EQ(result.encoderFormat, AV_PIX_FMT_VAAPI);
    ASSERT_GT(result.inputFrames, 0);
    ASSERT_EQ(result.outputFrames, result.inputFrames);
    ASSERT_EQ(result.outputSize, QSize(64, 64));
}

// Hardware filters feed a software encoder through a download.
TEST(FfmpegVideoTranscoder, VaapiFiltersBeforeSoftwareEncoder)
{
    if (!hasVaapi())
        GTEST_SKIP() << "No VAAPI device";

    nx::kit::IniConfig::Tweaks tweaks;
    tweaks.set(&nx::vms::common::ini().enableHwTranscoding, true);

    QnFfmpegVideoTranscoder::Config config;
    config.targetCodecId = AV_CODEC_ID_MJPEG;
    config.outputResolutionLimit = QSize(0, 64);
    const auto result = transcode(readFile(":/test_b_frames.mkv"), config);

    ASSERT_EQ(result.encoderFormat, AV_PIX_FMT_YUVJ420P);
    ASSERT_GT(result.inputFrames, 0);
    ASSERT_EQ(result.outputFrames, result.inputFrames);
    ASSERT_EQ(result.outputSize, QSize(64, 64));
}

TEST(FfmpegVideoTranscoder, FlushTwoChannels)
{
    for (const bool vaapi: {false, hasVaapi()})
    {
        SCOPED_TRACE(vaapi ? "VAAPI" : "software");
        nx::kit::IniConfig::Tweaks tweaks;
        tweaks.set(&nx::vms::common::ini().enableHwTranscoding, vaapi);

        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = AV_CODEC_ID_H264;
        config.outputResolutionLimit = QSize(0, 64);
        const auto result = transcode(readFile(":/test_b_frames.mkv"), config, {}, true);

        ASSERT_GT(result.inputFrames, 2);
        EXPECT_EQ(result.outputFrames, result.inputFrames);
    }
}

// Only the last frame is past the start time. On flush the decoder holds it behind one that is
// dropped, which must not end the flush. Device decoding only: the software decoder stamps the
// frames of this B-frame stream out of order.
TEST(FfmpegVideoTranscoder, FlushPastDroppedFrame)
{
    if (!hasVaapi())
        GTEST_SKIP() << "No VAAPI device";

    nx::kit::IniConfig::Tweaks tweaks;
    tweaks.set(&nx::vms::common::ini().enableHwTranscoding, true);

    const QByteArray media = readFile(":/test_b_frames.mkv");
    QnFfmpegVideoTranscoder::Config config;
    config.targetCodecId = AV_CODEC_ID_H264;
    config.outputResolutionLimit = QSize(0, 64);
    config.startTimeUs = lastFrameTime(media);
    ASSERT_EQ(transcode(media, config).outputFrames, 1);
}

// HEVC has only hardware encoders: without VAAPI, open() fails instead of trying one.
TEST(FfmpegVideoTranscoder, HevcWithoutVaapiFails)
{
    QnFfmpegVideoTranscoder::Config config;
    config.targetCodecId = AV_CODEC_ID_HEVC;
    config.sourceResolution = QSize(1920, 1080);
    QnFfmpegVideoTranscoder transcoder(config, nullptr);
    ASSERT_FALSE(transcoder.open(getVideoData()));
}

// Run manually: NX_TRANSCODING_BENCHMARK_FILE=<h264 file> --gtest_also_run_disabled_tests.
TEST(FfmpegVideoTranscoder, DISABLED_VaapiBenchmark)
{
    const char* const fileName = std::getenv("NX_TRANSCODING_BENCHMARK_FILE");
    if (!fileName)
        GTEST_SKIP() << "NX_TRANSCODING_BENCHMARK_FILE is not set";
    const QByteArray media = readFile(fileName);
    ASSERT_FALSE(media.isEmpty());

    QnFfmpegVideoTranscoder::Config config;
    config.targetCodecId = AV_CODEC_ID_H264;
    config.outputResolutionLimit = QSize(0, 360);
    config.useMultiThreadEncode = true;
    config.decoderConfig.mtDecodePolicy = MultiThreadDecodePolicy::enabled;

    for (const bool vaapi: {false, hasVaapi()})
    {
        nx::kit::IniConfig::Tweaks tweaks;
        tweaks.set(&nx::vms::common::ini().enableHwTranscoding, vaapi);
        const auto cpuStart = cpuTime();
        const auto wallStart = std::chrono::steady_clock::now();
        const auto result = transcode(media, config);
        const auto wall = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - wallStart);
        std::cout << (vaapi ? "VAAPI   " : "software") << ": " << result.inputFrames << " -> "
                  << result.outputFrames << " frames, " << result.outputSize.width() << "x"
                  << result.outputSize.height() << ", wall " << wall.count() << " ms, CPU "
                  << (cpuTime() - cpuStart).count() << " ms" << std::endl;
        ASSERT_EQ(result.outputFrames, result.inputFrames);
    }
}
