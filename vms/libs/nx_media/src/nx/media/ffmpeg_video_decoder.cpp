// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_video_decoder.h"

#include <nx/media/video_frame.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
} // extern "C"

#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/old_api.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "aligned_mem_video_buffer.h"
#include "avframe_memory_buffer.h"
#include "ini.h"

namespace nx {
namespace media {

namespace {

static const nx::log::Tag kLogTag(QString("FfmpegVideoDecoder"));
static const int kHeavyVideoThreshold = 1'000'000 * 2;

} // namespace


//-------------------------------------------------------------------------------------------------
// FfmpegDecoderPrivate

class FfmpegVideoDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(FfmpegVideoDecoder)
        FfmpegVideoDecoder* q_ptr;

public:
    FfmpegVideoDecoderPrivate()
        :
        codecContext(nullptr),
        frame(av_frame_alloc()),
        lastPts(AV_NOPTS_VALUE),
        scaleContext(nullptr)
    {
    }

    ~FfmpegVideoDecoderPrivate()
    {
        avcodec_free_context(&codecContext);
        av_frame_free(&frame);
        sws_freeContext(scaleContext);
    }

    void initContext(const QnConstCompressedVideoDataPtr& frame);

    // convert color space if QT doesn't support it
    static AVFrame* convertPixelFormat(const AVFrame* srcFrame, SwsContext** scaleContext);

    // Create video frame from AVFrame. If the ownership is taken the *pFrame is set to null.
    static VideoFrame* fromAVFrame(
        AVFrame** pFrame,
        SwsContext** scaleContext,
        bool tryToTakeOwnership = true);

    AVCodecContext* codecContext;
    AVFrame* frame;
    qint64 lastPts;
    SwsContext* scaleContext;
};

void FfmpegVideoDecoderPrivate::initContext(const QnConstCompressedVideoDataPtr& frame)
{
    if (!frame)
        return;

    NX_DEBUG(this, "Initialize Ffmpeg video decoder: %1", frame->compressionType);
    auto codec = avcodec_find_decoder(frame->compressionType);
    codecContext = avcodec_alloc_context3(codec);
    if (frame->context)
        frame->context->toAvCodecContext(codecContext);
    if (frame && frame->width * frame->height >= kHeavyVideoThreshold)
        codecContext->thread_count = 4;
    if (avcodec_open2(codecContext, codec, nullptr) < 0)
    {
        NX_DEBUG(kLogTag, nx::format("Can't open decoder for codec %1").arg(frame->compressionType));
        avcodec_free_context(&codecContext);
        return;
    }
}

AVFrame* FfmpegVideoDecoderPrivate::convertPixelFormat(
    const AVFrame* srcFrame,
    SwsContext** scaleContext)
{
    static const AVPixelFormat dstAvFormat = AV_PIX_FMT_YUV420P;

    if (!*scaleContext)
    {
        *scaleContext = sws_getContext(
            srcFrame->width, srcFrame->height, (AVPixelFormat)srcFrame->format,
            srcFrame->width, srcFrame->height, dstAvFormat,
            SWS_BICUBIC, nullptr, nullptr, nullptr);
    }

    // ffmpeg can return null context
    if (!*scaleContext)
        return nullptr;

    AVFrame* dstFrame = av_frame_alloc();
    int numBytes = av_image_get_buffer_size(dstAvFormat, srcFrame->linesize[0], srcFrame->height, /*align*/ 1);
    if (numBytes <= 0)
        return nullptr; //< can't allocate frame
    numBytes += AV_INPUT_BUFFER_PADDING_SIZE; //< extra alloc space due to ffmpeg doc
    dstFrame->buf[0] = av_buffer_alloc(numBytes);
    av_image_fill_arrays(
        dstFrame->data,
        dstFrame->linesize,
        dstFrame->buf[0]->data,
        dstAvFormat,
        srcFrame->linesize[0],
        srcFrame->height,
        /*align*/ 1);
    dstFrame->width = srcFrame->width;
    dstFrame->height = srcFrame->height;
    dstFrame->format = dstAvFormat;

    sws_scale(
        *scaleContext,
        srcFrame->data, srcFrame->linesize,
        0, srcFrame->height,
        dstFrame->data, dstFrame->linesize);

    return dstFrame;
}


//-------------------------------------------------------------------------------------------------
// FfmpegDecoder

QMap<int, QSize> FfmpegVideoDecoder::s_maxResolutions;

FfmpegVideoDecoder::FfmpegVideoDecoder(
    const QSize& /*resolution*/,
    QRhi* /*rhi*/)
    :
    AbstractVideoDecoder(),
    d_ptr(new FfmpegVideoDecoderPrivate())
{
}

FfmpegVideoDecoder::~FfmpegVideoDecoder()
{
}

bool FfmpegVideoDecoder::isCompatible(
    const AVCodecID codec,
    const QSize& resolution,
    bool allowHardwareAcceleration)
{
    if (!allowHardwareAcceleration)
        return true;

    const QSize maxRes = maxResolution(codec);
    if (maxRes.isEmpty())
        return true;

    const auto fixedResolution = resolution.height() > resolution.width()
        ? resolution.transposed()
        : resolution;

    if (fixedResolution.width() <= maxRes.width() && fixedResolution.height() <= maxRes.height())
        return true;

    NX_WARNING(kLogTag, nx::format("Max resolution %1 x %2 exceeded: %3 x %4").args(
        maxRes.width(), maxRes.height(), fixedResolution.width(), fixedResolution.height()));

    if (ini().unlimitFfmpegMaxResolution)
    {
        NX_WARNING(kLogTag, ".ini unlimitFfmpegMaxResolution is set => ignore limit");
        return true;
    }
    else
    {
        return false;
    }
}

QSize FfmpegVideoDecoder::maxResolution(const AVCodecID codec)
{
    QSize  result = s_maxResolutions.value(codec);
    if (!result.isEmpty())
        return result;
    return s_maxResolutions.value(AV_CODEC_ID_NONE);
}

VideoFrame* FfmpegVideoDecoderPrivate::fromAVFrame(
    AVFrame** pFrame,
    SwsContext** scaleContext,
    bool tryToTakeOwnership)
{
    qint64 startTimeMs = (*pFrame)->pkt_dts / 1000;

    const auto qtPixelFormat = AvFrameMemoryBuffer::toQtPixelFormat((AVPixelFormat) (*pFrame)->format);

    // Frame moved to the buffer. Buffer keeps reference to a frame.
    std::unique_ptr<QAbstractVideoBuffer> buffer;
    if (qtPixelFormat != QVideoFrameFormat::Format_Invalid)
    {
        buffer = std::make_unique<AvFrameMemoryBuffer>(*pFrame, tryToTakeOwnership);
        if (tryToTakeOwnership)
            *pFrame = nullptr;
    }
    else
    {
        AVFrame* newFrame = FfmpegVideoDecoderPrivate::convertPixelFormat(*pFrame, scaleContext);
        if (!newFrame)
            return nullptr; //< can't convert pixel format
        buffer = std::make_unique<AvFrameMemoryBuffer>(newFrame, /* ownsFrame */ true);
    }

    auto videoFrame = new VideoFrame(std::move(buffer));
    videoFrame->setStartTime(startTimeMs);
    return videoFrame;
}

VideoFrame* FfmpegVideoDecoder::fromAVFrame(const AVFrame* frame)
{
    SwsContext* scaleContext = nullptr;
    AVFrame* pFrame = const_cast<AVFrame*>(frame);

    auto videoFrame = FfmpegVideoDecoderPrivate::fromAVFrame(
        &pFrame, &scaleContext, /* ownsFrame */ false);

    if (scaleContext)
        sws_freeContext(scaleContext);

    return videoFrame;
}

bool FfmpegVideoDecoder::sendPacket(const QnConstCompressedVideoDataPtr& compressedVideoData)
{
    Q_D(FfmpegVideoDecoder);

    if (!d->codecContext)
    {
        d->initContext(compressedVideoData);
        if (!d->codecContext)
            return false; //< error
    }

    nx::media::ffmpeg::AvPacket avPacket(compressedVideoData.get());
    auto packet = avPacket.get();

    // There is a known ffmpeg bug. It returns the below time for the very last packet while
    // flushing internal buffer. So, repeat this time for the empty packet in order to avoid
    // the bug.
    if (compressedVideoData)
        d->lastPts = compressedVideoData->timestamp;
    else
        packet->pts = packet->dts = d->lastPts;

    int result = avcodec_send_packet(d->codecContext, packet);
    if (result == AVERROR(EAGAIN))
    {
        // We fully drain all the output in each decode call, so this should not ever happen.
        NX_WARNING(this, "Failed to send packet to decoder, impossible EAGAIN");
        return false;
    }
    if (result < 0)
    {
        NX_WARNING(this, "Failed to send packet to decoder, result: %1", result);
        return false;
    }

    return true;
}

bool FfmpegVideoDecoder::receiveFrame(VideoFramePtr* decodedFrame)
{
    Q_D(FfmpegVideoDecoder);
    if (!d->codecContext)
        return false;
    int result = avcodec_receive_frame(d->codecContext, d->frame);
    if (result < 0)
    {
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            return true;

        return false;
    }

    VideoFrame* videoFrame = FfmpegVideoDecoderPrivate::fromAVFrame(
        &d->frame,
        &d->scaleContext,
        /*tryToTakeOwnership*/ true);

    if (!d->frame) //< If the ownership was taken allocate a new frame.
        d->frame = av_frame_alloc();

    if (!videoFrame)
        return false;

    decodedFrame->reset(videoFrame);
    return true;
}

int FfmpegVideoDecoder::currentFrameNumber() const
{
    Q_D(const FfmpegVideoDecoder);
    return  qMax(0, d->codecContext->frame_num - 1);
}

double FfmpegVideoDecoder::getSampleAspectRatio() const
{
    Q_D(const FfmpegVideoDecoder);
    if (d->codecContext && d->codecContext->width > 8 && d->codecContext->height > 8)
    {
        double result = av_q2d(d->codecContext->sample_aspect_ratio);
        if (result > 1e-7)
            return result;
    }
    return 1.0;
}

void FfmpegVideoDecoder::setMaxResolutions(const QMap<int, QSize>& maxResolutions)
{
    s_maxResolutions = maxResolutions;
}

AbstractVideoDecoder::Capabilities FfmpegVideoDecoder::capabilities() const
{
    return Capability::noCapability;
}

} // namespace media
} // namespace nx
