#ifndef DISABLE_FFMPEG

#include "ffmpeg_video_decoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/videotoolbox.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/frame.h>
} // extern "C"

#include <utils/media/ffmpeg_helper.h>
#include <utils/media/ffmpeg_initializer.h>
#include <utils/media/nalUnits.h>
#include <nx/utils/thread/mutex.h>

#include "aligned_mem_video_buffer.h"

#include <QtCore/QFile>

#if defined(TARGET_OS_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

namespace nx {
namespace media {

QSize FfmpegVideoDecoder::s_maxResolution;    

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

    static enum AVPixelFormat get_format(AVCodecContext *s, const enum AVPixelFormat *pix_fmts)
    {
        int status = av_videotoolbox_default_init(s);
        return pix_fmts[0];
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
        frame(av_frame_alloc()),
        tmp_frame(av_frame_alloc()),
        lastPts(AV_NOPTS_VALUE)
    {
    }

    ~FfmpegVideoDecoderPrivate()
    {
        if (codecContext)
            av_videotoolbox_default_free(codecContext);

        closeCodecContext();
        av_frame_free(&tmp_frame);
        av_frame_free(&frame);
    }

    void initContext(const QnConstCompressedVideoDataPtr& frame);
    void closeCodecContext();

    AVCodecContext* codecContext;
    AVFrame* frame;
    AVFrame* tmp_frame;
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

    codecContext->thread_count = 1;
    codecContext->opaque = this;
    codecContext->get_format = get_format;
    codecContext->extradata_size = 1;
    
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

#if !defined(TARGET_OS_IPHONE)
    if (!d->frame->data[0])
        return res;
#else
    av_frame_unref(d->tmp_frame);
    
    CVPixelBufferRef pixbuf = (CVPixelBufferRef)d->frame->data[3];
    OSType pixel_format = CVPixelBufferGetPixelFormatType(pixbuf);

    switch (pixel_format) {
        case kCVPixelFormatType_420YpCbCr8Planar: d->tmp_frame->format = AV_PIX_FMT_YUV420P; break;
        case kCVPixelFormatType_422YpCbCr8:       d->tmp_frame->format = AV_PIX_FMT_UYVY422; break;
        case kCVPixelFormatType_32BGRA:           d->tmp_frame->format = AV_PIX_FMT_BGRA; break;
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: d->tmp_frame->format = AV_PIX_FMT_NV12; break;
        default:
            return AVERROR(ENOSYS);
    }
    
    d->tmp_frame->width  = d->frame->width;
    d->tmp_frame->height = d->frame->height;
    av_frame_get_buffer(d->tmp_frame, 32);
    
    CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    
    uint8_t *data[4] = { 0 };
    int linesize[4] = { 0 };
    int planes, ret, i;
    
    if (CVPixelBufferIsPlanar(pixbuf)) {
        
        planes = CVPixelBufferGetPlaneCount(pixbuf);
        for (i = 0; i < planes; i++) {
            data[i]     = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixbuf, i);
            linesize[i] = CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
        }
    } else {
        data[0] = (uint8_t*)CVPixelBufferGetBaseAddress(pixbuf);
        linesize[0] = CVPixelBufferGetBytesPerRow(pixbuf);
    }
    
    av_image_copy(d->tmp_frame->data, d->tmp_frame->linesize,
                  (const uint8_t **)data, linesize, (AVPixelFormat)d->tmp_frame->format,
                  d->frame->width, d->frame->height);
    
    ret = av_frame_copy_props(d->tmp_frame, d->frame);
    CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    if (ret < 0)
        return ret;
    
    av_frame_unref(d->frame);
    av_frame_move_ref(d->frame, d->tmp_frame);
#endif

    
    ffmpegToQtVideoFrame(outDecodedFrame);
    return d->frame->coded_picture_number;
}

void FfmpegVideoDecoder::ffmpegToQtVideoFrame(QVideoFramePtr* result)
{
    Q_D(FfmpegVideoDecoder);

    const int alignedWidth = qPower2Ceil((unsigned)d->frame->width, (unsigned)kMediaAlignment);
    const int numBytes = avpicture_get_size(AV_PIX_FMT_YUV420P, alignedWidth, d->frame->height);
    const int lineSize = alignedWidth;

    auto alignedBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, lineSize);
    auto videoFrame = new QVideoFrame(
        alignedBuffer, QSize(d->frame->width, d->frame->height), QVideoFrame::Format_NV12);
    // Ffmpeg pts/dts are mixed up here, so it's pkt_dts. Also Convert usec to msec.
    videoFrame->setStartTime(d->frame->pkt_dts / 1000);

    videoFrame->map(QAbstractVideoBuffer::WriteOnly);
    uchar* buffer = videoFrame->bits();

    
    const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat)d->frame->format);
    
    quint8* dstData[3];
    dstData[0] = buffer;
    dstData[1] = buffer + lineSize * d->frame->height;
    dstData[2] = dstData[1] + lineSize * d->frame->height / (descr->log2_chroma_w +1) / (descr->log2_chroma_h + 1);

    for (int i = 0; i < descr->nb_components; ++i)
    {
        if (!d->frame->data[i])
            break;
        int kx = 1;
        int ky = 1;
        if (i > 0)
        {
            kx <<= descr->log2_chroma_w;
            ky <<= descr->log2_chroma_h;
            kx /= descr->comp[i].step;
        }
        copyPlane(
            dstData[i],
            d->frame->data[i],
            d->frame->width / kx,
            lineSize / kx,
            d->frame->linesize[i],
            d->frame->height / ky);
    }

    videoFrame->unmap();

    result->reset(videoFrame);
}

void FfmpegVideoDecoder::setMaxResolution(const QSize& maxResolution)
{
    s_maxResolution = maxResolution;
}

} // namespace media
} // namespace nx

#endif // DISABLE_FFMPEG
