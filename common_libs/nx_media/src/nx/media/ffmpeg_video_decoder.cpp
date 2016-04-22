#ifndef DISABLE_FFMPEG

#include "ffmpeg_video_decoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
} // extern "C"

#include <utils/media/ffmpeg_helper.h>
#include <nx/utils/thread/mutex.h>

#include "aligned_mem_video_buffer.h"

namespace nx {
namespace media {

class InitFfmpegLib
{
public:

    InitFfmpegLib()
    {
        av_register_all();
        if (av_lockmgr_register(&InitFfmpegLib::lockmgr) != 0)
            qCritical() << "Failed to register ffmpeg lock manager";
    }

    static int lockmgr(void** mtx, enum AVLockOp op)
    {
        QnMutex** qMutex = (QnMutex**) mtx;
        switch (op)
        {
            case AV_LOCK_CREATE:
                *qMutex = new QnMutex();
                return 0;
            case AV_LOCK_OBTAIN:
                (*qMutex)->lock();
                return 0;
            case AV_LOCK_RELEASE:
                (*qMutex)->unlock();
                return 0;
            case AV_LOCK_DESTROY:
                delete *qMutex;
                return 0;
            default:
                return 1;
        }
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
        frame(avcodec_alloc_frame()),
        scaleContext(nullptr),
        lastPts(AV_NOPTS_VALUE)
    {
    }

    ~FfmpegVideoDecoderPrivate()
    {
        closeCodecContext();
        av_free(frame);
        sws_freeContext(scaleContext);
    }

    void initContext(const QnConstCompressedVideoDataPtr& frame);
    void closeCodecContext();

    AVCodecContext* codecContext;
    AVFrame* frame;
    SwsContext* scaleContext;
    qint64 lastPts;
    QSize scaleContextSize;
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
}

void FfmpegVideoDecoderPrivate::closeCodecContext()
{
    QnFfmpegHelper::deleteAvCodecContext(codecContext);
    codecContext = 0;
}

//-------------------------------------------------------------------------------------------------
// FfmpegDecoder

FfmpegVideoDecoder::FfmpegVideoDecoder()
:
    AbstractVideoDecoder(),
    d_ptr(new FfmpegVideoDecoderPrivate())
{
    static InitFfmpegLib init;
}

FfmpegVideoDecoder::~FfmpegVideoDecoder()
{
}

bool FfmpegVideoDecoder::isCompatible(const CodecID codec, const QSize& resolution)
{
    Q_UNUSED(codec);
    Q_UNUSED(resolution)
    return true;
}

int FfmpegVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame)
{
    Q_D(FfmpegVideoDecoder);

    if (!d->codecContext)
    {
        d->initContext(compressedVideoData);
        if (!d->codecContext)
            return -1;
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

    ffmpegToQtVideoFrame(outDecodedFrame);
    return d->frame->coded_picture_number;
}

void FfmpegVideoDecoder::ffmpegToQtVideoFrame(QVideoFramePtr* result)
{
    Q_D(FfmpegVideoDecoder);

    QSize size(d->frame->width, d->frame->height);
    if (size != d->scaleContextSize)
    {
        d->scaleContextSize = size;
        sws_freeContext(d->scaleContext);
        d->scaleContext = sws_getContext(
            d->frame->width, d->frame->height, (PixelFormat) d->frame->format,
            d->frame->width, d->frame->height, PIX_FMT_BGRA,
            SWS_BICUBIC, NULL, NULL, NULL);
    }
    const int alignedWidth = qPower2Ceil((unsigned)d->frame->width, (unsigned)kMediaAlignment);
    const int numBytes = avpicture_get_size(PIX_FMT_BGRA, alignedWidth, d->frame->height);
    const int argbLineSize = alignedWidth * 4;

    auto alignedBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, argbLineSize);
    auto videoFrame = new QVideoFrame(
        alignedBuffer, QSize(d->frame->width, d->frame->height), QVideoFrame::Format_RGB32);
    // Ffmpeg pts/dts are mixed up here, so it's pkt_dts. Also Convert usec to msec.
    videoFrame->setStartTime(d->frame->pkt_dts / 1000);

    videoFrame->map(QAbstractVideoBuffer::WriteOnly);
    uchar* buffer = videoFrame->bits();

    quint8* dstData[4];
    memset(dstData, 0, sizeof(dstData));
    dstData[0] = (quint8*)buffer;

    int dstLinesize[4];
    memset(dstLinesize, 0, sizeof(dstLinesize));
    dstLinesize[0] = argbLineSize;

    // Perform yuv2rgb transformation here.
    sws_scale(d->scaleContext, d->frame->data, d->frame->linesize, 0, d->frame->height, dstData,
        dstLinesize);
    videoFrame->unmap();

    result->reset(videoFrame);
}

} // namespace media
} // namespace nx

#endif // DISABLE_FFMPEG
