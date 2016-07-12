#include "ffmpeg_video_decoder.h"
#if !defined(DISABLE_FFMPEG)

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
};


#include <utils/media/ffmpeg_helper.h>
#include <nx/utils/thread/mutex.h>

#include "aligned_mem_video_buffer.h"
#include <QtMultimedia/QAbstractVideoBuffer>
#include <utils/common/qt_private_headers.h>
#include QT_PRIVATE_HEADER(QtMultimedia,qabstractvideobuffer_p.h)

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
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUVJ422P:
            return QVideoFrame::Format_YV12;
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
        frame(av_frame_alloc()),
        mapMode(QAbstractVideoBuffer::NotMapped)
    {
        av_frame_move_ref(frame, _frame);
    }

    virtual ~AvFrameMemoryBufferPrivate()
    {
        av_buffer_unref(&frame->buf[0]);
        av_frame_free(&frame);
    }

    virtual int map(
        QAbstractVideoBuffer::MapMode mode,
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
    AvFrameMemoryBuffer(AVFrame* frame)
    :
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
        AVPixelFormat pixFmt = (AVPixelFormat) d->frame->format;
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
        lastPts(AV_NOPTS_VALUE)
    {
    }

    ~FfmpegVideoDecoderPrivate()
    {
        closeCodecContext();
        av_free(frame);
    }

    void initContext(const QnConstCompressedVideoDataPtr& frame);
    void closeCodecContext();

    AVCodecContext* codecContext;
    AVFrame* frame;
    qint64 lastPts;
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
        qWarning() << "Can't open decoder for codec" << frame->compressionType;
        closeCodecContext();
        return;
    }

    // keep frame unless we call 'av_buffer_unref'
    codecContext->refcounted_frames = 1;
}

void FfmpegVideoDecoderPrivate::closeCodecContext()
{
    QnFfmpegHelper::deleteAvCodecContext(codecContext);
    codecContext = 0;
}

//-------------------------------------------------------------------------------------------------
// FfmpegDecoder

QSize FfmpegVideoDecoder::s_maxResolution;

FfmpegVideoDecoder::FfmpegVideoDecoder(
    const ResourceAllocatorPtr& allocator,
    const QSize& resolution)
    :
    AbstractVideoDecoder(),
    d_ptr(new FfmpegVideoDecoderPrivate())
{
    QN_UNUSED(allocator, resolution);
}

FfmpegVideoDecoder::~FfmpegVideoDecoder()
{
}

bool FfmpegVideoDecoder::isCompatible(const AVCodecID codec, const QSize& resolution)
{
    Q_UNUSED(codec);

    const QSize maxRes = maxResolution(codec);
    if (maxRes.isEmpty())
        return true;

    return resolution.width() <= maxRes.width() &&
            resolution.height() <= maxRes.height();
}

QSize FfmpegVideoDecoder::maxResolution(const AVCodecID codec)
{
    QN_UNUSED(codec);

    return s_maxResolution;
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

    AVPacket avpkt;
    av_init_packet(&avpkt);
    if (compressedVideoData)
    {
        avpkt.data = (unsigned char*) compressedVideoData->data();
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
        avpkt.data = nullptr;
        avpkt.size = 0;
    }

    int gotPicture = 0;
    int res = avcodec_decode_video2(d->codecContext, d->frame, &gotPicture, &avpkt);
    if (res <= 0 || !gotPicture)
        return res; //< Negative value means error, zero means buffering.

    QSize frameSize(d->frame->width, d->frame->height);
    AVPixelFormat pixelFormat = (AVPixelFormat) d->frame->format;
    qint64 startTimeMs = d->frame->pkt_dts / 1000;

    auto qtPixelFormat = toQtPixelFormat(pixelFormat);
    if (qtPixelFormat == QVideoFrame::Format_Invalid)
        return -1; //< report error

    QAbstractVideoBuffer* buffer = new AvFrameMemoryBuffer(d->frame); //< frame is moved here. null object after the call
    d->frame = av_frame_alloc();
    QVideoFrame* videoFrame = new QVideoFrame(buffer, frameSize, qtPixelFormat);
    videoFrame->setStartTime(startTimeMs);

    outDecodedFrame->reset(videoFrame);
    return d->frame->coded_picture_number;
}

void FfmpegVideoDecoder::setMaxResolution(const QSize& maxResolution)
{
    s_maxResolution = maxResolution;
}

} // namespace media
} // namespace nx

#endif // !DISABLE_FFMPEG
