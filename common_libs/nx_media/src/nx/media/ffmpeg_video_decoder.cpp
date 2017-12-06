#include <nx/utils/log/log.h>

#include "ini.h"
#include "ffmpeg_video_decoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
} // extern "C"

#include <utils/media/ffmpeg_helper.h>
#include <nx/utils/thread/mutex.h>

#include "aligned_mem_video_buffer.h"
#include <QtMultimedia/QAbstractVideoBuffer>
#include <QtMultimedia/private/qabstractvideobuffer_p.h>

namespace nx {
namespace media {

namespace {

QVideoFrame::PixelFormat toQtPixelFormat(AVPixelFormat pixFormat)
{
    switch (pixFormat)
    {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
            return QVideoFrame::Format_YUV420P;
        case AV_PIX_FMT_BGRA:
            return QVideoFrame::Format_BGRA32;
        case AV_PIX_FMT_NV12:
            return QVideoFrame::Format_NV12;
        default:
            return QVideoFrame::Format_Invalid;
    }
}

} // namespace

class AvFrameMemoryBufferPrivate: public QAbstractVideoBufferPrivate
{
public:
    AVFrame* frame;
    QAbstractVideoBuffer::MapMode mapMode;
public:
    AvFrameMemoryBufferPrivate(AVFrame* _frame):
        QAbstractVideoBufferPrivate(),
        frame(_frame),
        mapMode(QAbstractVideoBuffer::NotMapped)
    {
    }

    virtual ~AvFrameMemoryBufferPrivate()
    {
        av_frame_free(&frame);
    }

    virtual int map(
        QAbstractVideoBuffer::MapMode /*mode*/,
        int* numBytes,
        int linesize[4],
        uchar* data[4]) override
    {
        int planes = 0;
        *numBytes = 0;

        const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat)frame->format);

        for (int i = 0; i < 4 && frame->data[i]; ++i)
        {
            ++planes;
            data[i] = frame->data[i];
            linesize[i] = frame->linesize[i];

            int bytesPerPlane = linesize[i] * frame->height;
            if (i > 0)
            {
                bytesPerPlane >>= descr->log2_chroma_h + descr->log2_chroma_w;
                bytesPerPlane *= descr->comp->step;
            }
            *numBytes += bytesPerPlane;
        }
        return planes;
    }
};

class AvFrameMemoryBuffer: public QAbstractVideoBuffer
{
    Q_DECLARE_PRIVATE(AvFrameMemoryBuffer)
public:
    AvFrameMemoryBuffer(AVFrame* frame):
        QAbstractVideoBuffer(
            *(new AvFrameMemoryBufferPrivate(frame)),
            NoHandle)
    {
    }

    virtual MapMode mapMode() const override
    {
        return d_func()->mapMode;
    }

    virtual uchar* map(MapMode mode, int *numBytes, int *bytesPerLine) override
    {
        Q_D(AvFrameMemoryBuffer);
        *bytesPerLine = d->frame->linesize[0];
        AVPixelFormat pixFmt = (AVPixelFormat)d->frame->format;
        *numBytes = avpicture_get_size(pixFmt, d->frame->linesize[0], d->frame->height);
        return d->frame->data[0];
    }

    virtual void unmap() override
    {
    }
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
        closeCodecContext();
        av_frame_free(&frame);
        sws_freeContext(scaleContext);
    }

    void initContext(const QnConstCompressedVideoDataPtr& frame);
    void closeCodecContext();

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
        QnFfmpegHelper::mediaContextToAvCodecContext(codecContext, frame->context);
    //codecContext->thread_count = 4; //< Uncomment this line if decoder with internal buffer is required
    if (avcodec_open2(codecContext, codec, nullptr) < 0)
    {
        NX_LOG(lit("Can't open decoder for codec %1").arg(frame->compressionType), cl_logDEBUG1);
        closeCodecContext();
        return;
    }

    // keep frame unless we call 'av_frame_unref'
    codecContext->refcounted_frames = 1;
}

void FfmpegVideoDecoderPrivate::closeCodecContext()
{
    QnFfmpegHelper::deleteAvCodecContext(codecContext);
    codecContext = nullptr;
}

AVFrame* FfmpegVideoDecoderPrivate::convertPixelFormat(const AVFrame* srcFrame)
{
    static const AVPixelFormat dstAvFormat = AV_PIX_FMT_YUV420P;

    if (!scaleContext)
    {
        scaleContext = sws_getContext(
            srcFrame->width, srcFrame->height, (AVPixelFormat)srcFrame->format,
            srcFrame->width, srcFrame->height, dstAvFormat,
            SWS_BICUBIC, NULL, NULL, NULL);
    }

    AVFrame* dstFrame = av_frame_alloc();
    int numBytes = avpicture_get_size(dstAvFormat, srcFrame->linesize[0], srcFrame->height);
    if (numBytes <= 0)
        return nullptr; //< can't allocate frame
    numBytes += FF_INPUT_BUFFER_PADDING_SIZE; //< extra alloc space due to ffmpeg doc
    dstFrame->buf[0] = av_buffer_alloc(numBytes);
    avpicture_fill(
        (AVPicture*)dstFrame,
        dstFrame->buf[0]->data,
        dstAvFormat,
        srcFrame->linesize[0],
        srcFrame->height);
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

QMap<AVCodecID, QSize> FfmpegVideoDecoder::s_maxResolutions;

FfmpegVideoDecoder::FfmpegVideoDecoder(
    const ResourceAllocatorPtr& /*allocator*/,
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
    const AVCodecID codec, const QSize& resolution, bool /*allowOverlay*/)
{
    const QSize maxRes = maxResolution(codec);
    if (maxRes.isEmpty())
        return true;

    if (resolution.width() <= maxRes.width() && resolution.height() <= maxRes.height())
        return true;

    NX_LOG(lit("[ffmpeg_video_decoder] Max resolution %1 x %2 exceeded: %3 x %4")
        .arg(maxRes.width()).arg(maxRes.height())
        .arg(resolution.width()).arg(resolution.height()),
        cl_logWARNING);

    if (ini().unlimitFfmpegMaxResolution)
    {
        NX_LOG(lit(
            "[ffmpeg_video_decoder] .ini unlimitFfmpegMaxResolution is set => ignore limit"),
            cl_logWARNING);
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
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame)
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

        // It's already guaranteed by QnByteArray that there is an extra space reserved. We must
        // fill the padding bytes according to ffmpeg documentation.
        if (avpkt.data)
        {
            static_assert(QN_BYTE_ARRAY_PADDING >= FF_INPUT_BUFFER_PADDING_SIZE,
                "FfmpegVideoDecoder: Insufficient padding size");
            memset(avpkt.data + avpkt.size, 0, FF_INPUT_BUFFER_PADDING_SIZE);
        }

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
    if (qtPixelFormat != QVideoFrame::Format_Invalid)
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

    QVideoFrame* videoFrame = new QVideoFrame(buffer, frameSize, qtPixelFormat);
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

void FfmpegVideoDecoder::setMaxResolutions(const QMap<AVCodecID, QSize>& maxResolutions)
{
    s_maxResolutions = maxResolutions;
}

AbstractVideoDecoder::Capabilities FfmpegVideoDecoder::capabilities() const
{
    return Capability::noCapability;
}

} // namespace media
} // namespace nx
