#ifndef DISABLE_FFMPEG

#include "ffmpeg_video_decoder.h"

extern "C" {
#include <libavformat/avformat.h>
} // extern "C"

#include <utils/media/ffmpeg_helper.h>
#include <utils/media/ffmpeg_initializer.h>

#include <nx/utils/thread/mutex.h>

#include "aligned_mem_video_buffer.h"

#include <QtCore/QFile>

namespace nx {
namespace media {

namespace {

    void copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride, int src_stride, int height)
    {
        for (int i = 0; i < height; ++i)
        {
            memcpy(dst, src, width);
            dst += dst_stride;
            src += src_stride;
        }
    }

}


class InitFfmpegLib
{
public:
    //TODO: #rvasilenko replace this static class with QnFfmpegInitializer instance,
    // which will be destroyed in correct time.
    // Now av_lockmgr_register(nullptr) is not called in htis project at all
    InitFfmpegLib()
    {
        //QnFfmpegInitializer* initializer = new QnFfmpegInitializer()
        av_register_all();
        if (av_lockmgr_register(&InitFfmpegLib::lockmgr) != 0)
            qCritical() << "Failed to register ffmpeg lock manager";
    }

    static int lockmgr(void** mtx, enum AVLockOp op)
    {
        QnMutex** qMutex = (QnMutex**)mtx;
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

    QByteArray testData;
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

FfmpegVideoDecoder::FfmpegVideoDecoder(
    const ResourceAllocatorPtr& allocator, const QSize& resolution)
:
    AbstractVideoDecoder(),
    d_ptr(new FfmpegVideoDecoderPrivate())
{
    QN_UNUSED(allocator, resolution);
    static InitFfmpegLib init;
    QN_UNUSED(init);
}

FfmpegVideoDecoder::~FfmpegVideoDecoder()
{
}

bool FfmpegVideoDecoder::isCompatible(const AVCodecID codec, const QSize& resolution)
{
    Q_UNUSED(codec);
    Q_UNUSED(resolution)
    return true;
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

#if 0
    // test data
    const QVideoFrame::PixelFormat Format_Tiled32x32NV12 = QVideoFrame::PixelFormat(QVideoFrame::Format_User + 17);

    if (d->testData.isEmpty())
    {
        QFile file("f:\\yuv_frames\\frame_1920x1080_26_native.dat");
        file.open(QIODevice::ReadOnly);
        d->testData = file.readAll();
    }

    quint8* srcData[4];
    srcData[0] = (quint8*) d->testData.data();
    srcData[1] = srcData[0] + qPower2Ceil((unsigned) d->frame->width, 32) * qPower2Ceil((unsigned) d->frame->height, 32);

    int srcLineSize[4];
    srcLineSize[0] = srcLineSize[1] = qPower2Ceil((unsigned) d->frame->width, 32);

    auto alignedBuffer = new AlignedMemVideoBuffer(srcData, srcLineSize, 2); //< two planes buffer
    auto videoFrame = new QVideoFrame(alignedBuffer, QSize(d->frame->width, d->frame->height), Format_Tiled32x32NV12);

    // Ffmpeg pts/dts are mixed up here, so it's pkt_dts. Also Convert usec to msec.
    videoFrame->setStartTime(d->frame->pkt_dts / 1000);

#else
    const int alignedWidth = qPower2Ceil((unsigned)d->frame->width, (unsigned)kMediaAlignment);
    const int numBytes = avpicture_get_size(AV_PIX_FMT_YUV420P, alignedWidth, d->frame->height);
    const int lineSize = alignedWidth;

    auto alignedBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, lineSize);
    auto videoFrame = new QVideoFrame(
        alignedBuffer, QSize(d->frame->width, d->frame->height), QVideoFrame::Format_YUV420P);
    // Ffmpeg pts/dts are mixed up here, so it's pkt_dts. Also Convert usec to msec.
    videoFrame->setStartTime(d->frame->pkt_dts / 1000);

    videoFrame->map(QAbstractVideoBuffer::WriteOnly);
    uchar* buffer = videoFrame->bits();

    quint8* dstData[3];
    dstData[0] = buffer;
    dstData[1] = buffer + lineSize * d->frame->height;
    dstData[2] = dstData[1] + lineSize * d->frame->height / 4;

    for (int i = 0; i < 3; ++i)
    {
        const int k = (i == 0 ? 0 : 1);
        copyPlane(
            dstData[i],
            d->frame->data[i],
            d->frame->width >> k,
            lineSize >> k,
            d->frame->linesize[i],
            d->frame->height >> k);
    }

    videoFrame->unmap();
#endif

    result->reset(videoFrame);
}

void FfmpegVideoDecoder::setMaxResolution(const QSize& maxResolution)
{
    s_maxResolution = maxResolution;
}

} // namespace media
} // namespace nx

#endif // DISABLE_FFMPEG
