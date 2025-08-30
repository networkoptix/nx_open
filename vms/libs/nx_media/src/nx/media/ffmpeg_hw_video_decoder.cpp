// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_hw_video_decoder.h"

#include <QtGui/rhi/qrhi.h>
#include <QtMultimedia/QVideoFrameFormat>

#include <nx/build_info.h>
#include <nx/media/annexb_to_mp4.h>
#include <nx/media/avframe_memory_buffer.h>
#include <nx/media/ffmpeg/hw_video_api.h>
#include <nx/media/ffmpeg/hw_video_decoder.h>
#include <nx/media/h264_utils.h>
#include <nx/media/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

namespace nx::media {

namespace {

AVHWDeviceType deviceTypeFromRhi(QRhi* rhi)
{
    if (!rhi)
        return AV_HWDEVICE_TYPE_NONE;

    switch (rhi->backend())
    {
        case QRhi::Implementation::D3D11:
            return AV_HWDEVICE_TYPE_D3D11VA;
        case QRhi::Implementation::D3D12:
            return AV_HWDEVICE_TYPE_D3D12VA;
        case QRhi::Implementation::Metal:
            return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
        case QRhi::Vulkan:
            if (nx::build_info::isAndroid())
                return AV_HWDEVICE_TYPE_MEDIACODEC;
            return AV_HWDEVICE_TYPE_VULKAN;
        case QRhi::Implementation::OpenGLES2:
            if (nx::build_info::isAndroid())
                return AV_HWDEVICE_TYPE_MEDIACODEC;
            if (nx::build_info::isLinux())
                return AV_HWDEVICE_TYPE_VAAPI;
            [[fallthrough]];
        default:
            return AV_HWDEVICE_TYPE_NONE;
    }
}

}

class FfmpegHwVideoDecoder::Private
{
public:
    Private(QRhi* rhi): initialized(false), rhi(rhi)
    {
    }

    ~Private()
    {
        if (decoder)
            closeCodecContext();
    }

    void initContext(const QnConstCompressedVideoDataPtr& frame)
    {
        if (!frame || !frame->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
            return;

        if (!nx::media::ffmpeg::HwVideoDecoder::isCompatible(
            frame->compressionType,
            QSize(frame->width, frame->height),
            /*allowHardwareAcceleration*/ true))
        {
            NX_WARNING(this, "Decoder is not compatible with codec %1 resolution %2x%3",
                frame->compressionType, frame->width, frame->height);
            return;
        }

        NX_INFO(this, "Initializing decoder for codec %1 resolution %2x%3...",
            frame->compressionType, frame->width, frame->height);

        AVHWDeviceType hwType = deviceTypeFromRhi(rhi);

        auto api = VideoApiRegistry::instance()->get(hwType);

        std::shared_ptr<VideoApiDecoderData> data;
        if (api)
            data = api->createDecoderData(rhi);

        std::unique_ptr<nx::media::ffmpeg::AvOptions> options;
        nx::media::ffmpeg::HwVideoDecoder::InitFunc initFunc;
        std::string device;

        if (!api)
        {
            NX_DEBUG(this, "Switching to FFmpeg SW video decoder");
            hwType = AV_HWDEVICE_TYPE_NONE;
            initFunc = {};
        }
        else
        {
            initFunc = api->initFunc(rhi, data);
            options = api->options(rhi);
            device = api->device(rhi);
        }

        decoder = std::make_unique<nx::media::ffmpeg::HwVideoDecoder>(
            hwType,
            /* metrics */ nullptr,
            device,
            std::move(options),
            initFunc);
        decoderData = data;
        videoApi = api;
    }

    void closeCodecContext()
    {
        frameSize = {};
        decoder.reset();
        decoderData.reset();
    }

    std::unique_ptr<nx::media::ffmpeg::HwVideoDecoder> decoder;
    std::shared_ptr<VideoApiDecoderData> decoderData;
    AnnexbToMp4 m_annexbToMp4;

    bool initialized = false;
    QSize frameSize;

    QRhi* rhi = nullptr;
    VideoApiRegistry::Entry* videoApi = nullptr;
};

FfmpegHwVideoDecoder::FfmpegHwVideoDecoder(
    const QSize& /*resolution*/,
    QRhi* rhi)
    :
    AbstractVideoDecoder(),
    d(new Private(rhi))
{
}

FfmpegHwVideoDecoder::~FfmpegHwVideoDecoder()
{
}

bool FfmpegHwVideoDecoder::isCompatible(
    const AVCodecID codec,
    const QSize& resolution,
    bool allowHardwareAcceleration)
{
    if (!allowHardwareAcceleration)
        return false;

    const auto maxSize = FfmpegHwVideoDecoder::maxResolution(codec);
    if (!maxSize.isEmpty()
        && (resolution.width() > maxSize.width() || resolution.height() > maxSize.height()))
    {
        return false;
    }

    return (bool) avcodec_find_decoder(codec);
}

QSize FfmpegHwVideoDecoder::maxResolution(const AVCodecID /*codec*/)
{
    return {8192, 8192};
}

bool FfmpegHwVideoDecoder::sendPacket(const QnConstCompressedVideoDataPtr& packet)
{
    auto compressedVideoData = packet;
    if (packet && nx::media::isAnnexb(packet.get()))
    {
        compressedVideoData = d->m_annexbToMp4.process(packet.get());
        if (!compressedVideoData)
        {
            NX_WARNING(this, "Decoding failed, cannot convert to MP4 format");
            return false;
        }
    }

    if (!d->decoder)
    {
        d->initContext(compressedVideoData);
        if (!d->decoder)
            return false;
    }

    return d->decoder->sendPacket(compressedVideoData);
}

bool FfmpegHwVideoDecoder::receiveFrame(VideoFramePtr* decodedFrame)
{
    if (!d->decoder)
    {
        NX_WARNING(this, "Decoder is not initialized");
        return false;
    }

    CLVideoDecoderOutputPtr frameFromDecoder(new CLVideoDecoderOutput());
    if (!d->decoder->receiveFrame(&frameFromDecoder) || frameFromDecoder->format == -1)
    {
        const bool flushed = d->decoder->getLastDecodeResult() == 0;
        if (flushed)
            decodedFrame->reset();
        return flushed;
    }

    const qint64 startTimeMs = frameFromDecoder->pts == DATETIME_INVALID
        ? DATETIME_INVALID
        : frameFromDecoder->pts / 1000; //< Convert usec to msec.

    const auto pixelFormat = static_cast<AVPixelFormat>(frameFromDecoder->format);

    const bool isHW = d->videoApi && d->decoder->hardwareDecoder();

    if (isHW)
    {
        *decodedFrame = d->videoApi->makeFrame(frameFromDecoder.get(), d->decoderData);
        if (*decodedFrame)
            return true;
    }
    else if (AvFrameMemoryBuffer::toQtPixelFormat(pixelFormat) != QVideoFrameFormat::Format_Invalid)
    {
        // Software frame format.

        auto videoFrame = std::make_shared<VideoFrame>(
            std::make_unique<CLVideoDecoderOutputMemBuffer>(frameFromDecoder));
        videoFrame->setStartTime(startTimeMs);
        *decodedFrame = videoFrame;
        return true;
    }

    return false;
}

int FfmpegHwVideoDecoder::currentFrameNumber() const
{
    if (!d->decoder)
        return -1;

    return d->decoder->currentFrameNumber();
}

AbstractVideoDecoder::Capabilities FfmpegHwVideoDecoder::capabilities() const
{
    if (d->decoder && !d->decoder->hardwareDecoder())
        return Capability::noCapability;

    return Capability::hardwareAccelerated;
}

} // namespace nx::media
