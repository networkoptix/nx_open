// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_video_decoder.h"

#include <nx/media/video_frame.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
} // extern "C"

#include <nx/media/ffmpeg_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "aligned_mem_video_buffer.h"
#include "ini.h"

namespace nx {
namespace media {

namespace {

static const nx::utils::log::Tag kLogTag(QString("FfmpegVideoDecoder"));

QVideoFrameFormat::PixelFormat toQtPixelFormat(AVPixelFormat pixFormat)
{
    switch (pixFormat)
    {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
            return QVideoFrameFormat::Format_YUV420P;
        case AV_PIX_FMT_BGRA:
            return QVideoFrameFormat::Format_BGRA8888;
        case AV_PIX_FMT_NV12:
            return QVideoFrameFormat::Format_NV12;
        default:
            return QVideoFrameFormat::Format_Invalid;
    }
}

} // namespace


class AvFrameMemoryBuffer: public QAbstractVideoBuffer
{
public:
    AvFrameMemoryBuffer(AVFrame* frame):
        QAbstractVideoBuffer(QVideoFrame::NoHandle),
        m_frame(frame)
    {
    }

    virtual ~AvFrameMemoryBuffer()
    {
        av_frame_free(&m_frame);
    }

    virtual QVideoFrame::MapMode mapMode() const override
    {
        return m_mapMode;
    }

    virtual MapData map(QVideoFrame::MapMode mode) override
    {
        MapData data;

        if (mode != QVideoFrame::ReadOnly)
            return data;

        m_mapMode = mode;

        const auto pixelFormat = toQtPixelFormat((AVPixelFormat) m_frame->format);

        int planes = 0;

        for (int i = 0; i < 4 && m_frame->data[i]; ++i)
        {
            ++planes;
            data.data[i] = m_frame->data[i];
            data.bytesPerLine[i] = m_frame->linesize[i];
            int bytesPerPlane = data.bytesPerLine[i] * m_frame->height;

            if (i > 0 && pixelFormat == QVideoFrameFormat::Format_YUV420P)
                bytesPerPlane /= 2;

            data.size[i] = bytesPerPlane;
        }
        data.nPlanes = planes;

        return data;
    }

    virtual void unmap() override
    {
        m_mapMode = QVideoFrame::NotMapped;
    }

private:
    AVFrame* m_frame = nullptr;
    QVideoFrame::MapMode m_mapMode = QVideoFrame::NotMapped;
};

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
    AVFrame* convertPixelFormat(const AVFrame* srcFrame);

    AVCodecContext* codecContext;
    AVFrame* frame;
    qint64 lastPts;
    SwsContext* scaleContext;
};

void FfmpegVideoDecoderPrivate::initContext(const QnConstCompressedVideoDataPtr& frame)
{
    if (!frame)
        return;

    auto codec = avcodec_find_decoder(frame->compressionType);
    codecContext = avcodec_alloc_context3(codec);
    if (frame->context)
        frame->context->toAvCodecContext(codecContext);
    //codecContext->thread_count = 4; //< Uncomment this line if decoder with internal buffer is required
    if (avcodec_open2(codecContext, codec, nullptr) < 0)
    {
        NX_DEBUG(kLogTag, nx::format("Can't open decoder for codec %1").arg(frame->compressionType));
        avcodec_free_context(&codecContext);
        return;
    }

    // keep frame unless we call 'av_frame_unref'
    codecContext->refcounted_frames = 1;
}

AVFrame* FfmpegVideoDecoderPrivate::convertPixelFormat(const AVFrame* srcFrame)
{
    static const AVPixelFormat dstAvFormat = AV_PIX_FMT_YUV420P;

    if (!scaleContext)
    {
        scaleContext = sws_getContext(
            srcFrame->width, srcFrame->height, (AVPixelFormat)srcFrame->format,
            srcFrame->width, srcFrame->height, dstAvFormat,
            SWS_BICUBIC, nullptr, nullptr, nullptr);
    }

    // ffmpeg can return null context
    if (!scaleContext)
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
        scaleContext,
        srcFrame->data, srcFrame->linesize,
        0, srcFrame->height,
        dstFrame->data, dstFrame->linesize);

    return dstFrame;
}


//-------------------------------------------------------------------------------------------------
// FfmpegDecoder

QMap<int, QSize> FfmpegVideoDecoder::s_maxResolutions;

FfmpegVideoDecoder::FfmpegVideoDecoder(
    const RenderContextSynchronizerPtr& /*synchronizer*/,
    const QSize& /*resolution*/)
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
    bool /*allowOverlay*/,
    bool /*allowHardwareAcceleration*/)
{
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

int FfmpegVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, VideoFramePtr* outDecodedFrame)
{
    Q_D(FfmpegVideoDecoder);

    if (!d->codecContext)
    {
        d->initContext(compressedVideoData);
        if (!d->codecContext)
            return -1; //< error
    }

    QnFfmpegAvPacket avpkt;
    if (compressedVideoData)
    {
        avpkt.data = (unsigned char*)compressedVideoData->data();
        avpkt.size = static_cast<int>(compressedVideoData->dataSize());
        avpkt.dts = avpkt.pts = compressedVideoData->timestamp;
        if (compressedVideoData->flags & QnAbstractMediaData::MediaFlags_AVKey)
            avpkt.flags = AV_PKT_FLAG_KEY;

        // It's already guaranteed by nx::utils::ByteArray that there is an extra space reserved. We must
        // fill the padding bytes according to ffmpeg documentation.
        if (avpkt.data)
            memset(avpkt.data + avpkt.size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

        d->lastPts = compressedVideoData->timestamp;
    }
    else
    {
        // There is a known ffmpeg bug. It returns the below time for the very last packet while
        // flushing internal buffer. So, repeat this time for the empty packet in order to avoid
        // the bug.
        avpkt.pts = avpkt.dts = d->lastPts;
    }

    int gotPicture = 0;
    int res = avcodec_decode_video2(d->codecContext, d->frame, &gotPicture, &avpkt);
    if (res <= 0 || !gotPicture)
        return res; //< Negative value means error, zero means buffering.

    QSize frameSize(d->frame->width, d->frame->height);
    qint64 startTimeMs = d->frame->pkt_dts / 1000;
    int frameNum = qMax(0, d->codecContext->frame_number - 1);

    auto qtPixelFormat = toQtPixelFormat((AVPixelFormat)d->frame->format);

    // Frame moved to the buffer. buffer keeps reference to a frame.
    QAbstractVideoBuffer* buffer;
    if (qtPixelFormat != QVideoFrameFormat::Format_Invalid)
    {
        buffer = new AvFrameMemoryBuffer(d->frame);
        d->frame = av_frame_alloc();
    }
    else
    {
        AVFrame* newFrame = d->convertPixelFormat(d->frame);
        if (!newFrame)
            return -1; //< can't convert pixel format
        qtPixelFormat = toQtPixelFormat((AVPixelFormat)newFrame->format);
        buffer = new AvFrameMemoryBuffer(newFrame);
    }

    VideoFrame* videoFrame = new VideoFrame(buffer, {frameSize, qtPixelFormat});
    videoFrame->setStartTime(startTimeMs);

    outDecodedFrame->reset(videoFrame);
    return frameNum;
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
