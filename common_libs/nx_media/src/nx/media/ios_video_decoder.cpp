#if defined (Q_OS_IOS)

#include "ios_video_decoder.h"

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
#include <QtCore/QDebug>

#if defined(TARGET_OS_IPHONE)
#include <CoreVideo/CoreVideo.h>
#include "ios_device_info.h"
#endif

#include <QtMultimedia/QAbstractVideoBuffer>
#include <QtMultimedia/private/qabstractvideobuffer_p.h>

namespace nx {
namespace media {

namespace {
    
    bool isValidFrameSize(const QSize& size)
    {
        static const auto kMinimumFrameSize = QSize(64, 64);
        return size.width() >= kMinimumFrameSize.width()
        && size.height() >= kMinimumFrameSize.height();
    }

    bool isFatalError(int ffmpegErrorCode)
    {
        static const int kHWAcceleratorFailed = 0xb1b4b1ab;

        return ffmpegErrorCode == kHWAcceleratorFailed;
    }

    static enum AVPixelFormat get_format(AVCodecContext *s, const enum AVPixelFormat *pix_fmts)
    {
        int status = av_videotoolbox_default_init(s);
        if (status != 0)
            qWarning() << "IOS hardware decoder init failure:" << status;
        return pix_fmts[0];
    }

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

    QVideoFrame::PixelFormat toQtPixelFormat(OSType pixFormat)
    {
        switch (pixFormat)
        {
            case kCVPixelFormatType_420YpCbCr8Planar:
                return QVideoFrame::Format_YUV420P;
            case kCVPixelFormatType_422YpCbCr8:
                return QVideoFrame::Format_YV12;
            case kCVPixelFormatType_32BGRA:
                return QVideoFrame::Format_BGRA32;
            case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
                return QVideoFrame::Format_NV12;
            default:
                qWarning() << "unknown OSType format " << (int) pixFormat;
                return QVideoFrame::Format_Invalid;
        }
    }
}

class IOSMemoryBufferPrivate: public QAbstractVideoBufferPrivate
{
public:
    AVFrame* frame;
    QAbstractVideoBuffer::MapMode mapMode;
public:
    IOSMemoryBufferPrivate(AVFrame* _frame):
        QAbstractVideoBufferPrivate(),
        frame(_frame),
        mapMode(QAbstractVideoBuffer::NotMapped)
    {
    }

    virtual ~IOSMemoryBufferPrivate()
    {
        av_frame_free(&frame);
    }

    virtual int map(
                    QAbstractVideoBuffer::MapMode mode,
                    int* numBytes,
                    int linesize[4],
                    uchar* data[4]) override
    {
        int planes = 1;
        *numBytes = 0;

        if (frame->data[3])
        {
            // map frame to the internal buffer
            CVPixelBufferRef pixbuf = (CVPixelBufferRef)frame->data[3];
            CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);

            if (CVPixelBufferIsPlanar(pixbuf))
            {
                planes = CVPixelBufferGetPlaneCount(pixbuf);
                for (int i = 0; i < planes; i++)
                {
                    data[i]     = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixbuf, i);
                    linesize[i] = CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
                    *numBytes += linesize[i] * CVPixelBufferGetHeightOfPlane(pixbuf, i);
                }
            }
            else
            {
                data[0] = (uchar*) CVPixelBufferGetBaseAddress(pixbuf);
                linesize[0] = CVPixelBufferGetBytesPerRow(pixbuf);
                *numBytes = linesize[0] * CVPixelBufferGetHeight(pixbuf);
            }
        }
        else
        {
            // map frame to the ffmpeg buffer
            planes = 0;
            const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) frame->format);
            for (int i = 0; i < descr->nb_components && frame->data[i]; ++i)
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

        }

        return planes;
    }
};


class IOSMemoryBuffer: public QAbstractVideoBuffer
{
    Q_DECLARE_PRIVATE(IOSMemoryBuffer)
public:
    IOSMemoryBuffer(AVFrame* frame):
        QAbstractVideoBuffer(
                             *(new IOSMemoryBufferPrivate(frame)),
                             NoHandle)
    {
    }

    virtual MapMode mapMode() const override
    {
        return d_func()->mapMode;
    }

    virtual uchar* map(MapMode mode, int *numBytes, int *bytesPerLine) override
    {
        Q_D(IOSMemoryBuffer);
        CVPixelBufferRef pixbuf = (CVPixelBufferRef) d->frame->data[3];
        *bytesPerLine = CVPixelBufferGetBytesPerRow(pixbuf);
        *numBytes = *bytesPerLine * CVPixelBufferGetHeight(pixbuf);
        return (uchar*) CVPixelBufferGetBaseAddress(pixbuf);
    }

    virtual void unmap() override
    {
        Q_D(IOSMemoryBuffer);
        CVPixelBufferRef pixbuf = (CVPixelBufferRef) d->frame->data[3];
        CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    }
};

//-------------------------------------------------------------------------------------------------
// FfmpegDecoderPrivate

class IOSVideoDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(IOSVideoDecoder)
    IOSVideoDecoder* q_ptr;

public:
    IOSVideoDecoderPrivate()
    :
        codecContext(nullptr),
        frame(av_frame_alloc()),
        lastPts(AV_NOPTS_VALUE)
    {
    }

    ~IOSVideoDecoderPrivate()
    {
        closeCodecContext();
        av_frame_free(&frame);
    }

    void initContext(const QnConstCompressedVideoDataPtr& frame);
    void closeCodecContext();

    AVCodecContext* codecContext;
    AVFrame* frame;
    qint64 lastPts;
    bool isHardwareAccelerated = false;
};

void IOSVideoDecoderPrivate::initContext(const QnConstCompressedVideoDataPtr& frame)
{
    if (!frame)
        return;

    auto codec = avcodec_find_decoder(frame->compressionType);
    codecContext = avcodec_alloc_context3(codec);
    if (frame->context)
        QnFfmpegHelper::mediaContextToAvCodecContext(codecContext, frame->context);
    QSize frameSize = QSize(codecContext->width, codecContext->height);
    if (!isValidFrameSize(frameSize))
    {
        frameSize = QSize(frame->width, frame->height);
        if (!isValidFrameSize(frameSize))
            frameSize = nx::media::AbstractVideoDecoder::mediaSizeFromRawData(frame);
        codecContext->width = frameSize.width();
        codecContext->height = frameSize.height();
    }
    
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

    // keep frame unless we call 'av_buffer_unref'
    codecContext->refcounted_frames = 1;
}

void IOSVideoDecoderPrivate::closeCodecContext()
{
    if (codecContext)
        av_videotoolbox_default_free(codecContext);

    QnFfmpegHelper::deleteAvCodecContext(codecContext);
    codecContext = 0;
}

//-------------------------------------------------------------------------------------------------
// FfmpegDecoder

IOSVideoDecoder::IOSVideoDecoder(
    const ResourceAllocatorPtr& /*allocator*/, const QSize& /*resolution*/)
:
    AbstractVideoDecoder(),
    d_ptr(new IOSVideoDecoderPrivate())
{
}

IOSVideoDecoder::~IOSVideoDecoder()
{
}

bool IOSVideoDecoder::isCompatible(
    const AVCodecID codec,
    const QSize& resolution,
    bool /*allowOverlay*/)
{
    // VideoToolBox supports H263 only.
    // If cheat it and provide H263P instead (by changing compression type H263P -> H263)
    // it would show green box.
    if (codec != AV_CODEC_ID_H265 &&
        codec != AV_CODEC_ID_H264 &&
        codec != AV_CODEC_ID_H263P &&
        codec != AV_CODEC_ID_MPEG4 &&
        codec != AV_CODEC_ID_MPEG1VIDEO &&
        codec != AV_CODEC_ID_MPEG2VIDEO)
    {
        return false;
    }
    const QSize maxRes = maxResolution(codec);
    return resolution.width() <= maxRes.width() &&
           resolution.height() <= maxRes.height();
}

QSize IOSVideoDecoder::maxResolution(const AVCodecID codec)
{
    static const QSize kHdReadyResolution(1280, 720);
    static const QSize kFullHdResolution(1920, 1080);
    static const QSize kUhd4kResolution(3840, 2160);

    const auto& deviceInfo = iosDeviceInformation();
    switch (codec)
    {
        case AV_CODEC_ID_H265:
            // List of models with HEVC:
            // https://support.apple.com/en-ie/HT207022
            // https://stackoverflow.com/questions/11197509/how-to-get-device-make-and-model-on-ios
            if (deviceInfo.type == IosDeviceInformation::Type::iPhone)
            {
                if (deviceInfo.majorVersion > 7)
                    return kUhd4kResolution;
            }
            else if (deviceInfo.type == IosDeviceInformation::Type::iPad)
            {
                if (deviceInfo.majorVersion > 5)
                    return kUhd4kResolution;
            }
            return QSize(); //< HEVC is not supported.
        case AV_CODEC_ID_H264:
            if (deviceInfo.type == IosDeviceInformation::Type::iPhone)
            {
                if (deviceInfo.majorVersion >= 7) //< iPhone 6 and newer.
                    return kUhd4kResolution;
            }
            else if (deviceInfo.type == IosDeviceInformation::Type::iPad)
            {
                if (deviceInfo.majorVersion >= 5) //< iPad Air 2 / iPad Mini 4 or newer.
                    return kUhd4kResolution;
            }
            return kFullHdResolution;
        default:
            return kHdReadyResolution;
    }
}

int IOSVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData,
    QVideoFramePtr* outDecodedFrame)
{
    Q_D(IOSVideoDecoder);

    if (!d->codecContext)
    {
        d->initContext(compressedVideoData);
        if (!d->codecContext)
            return -1;
    }

    QnFfmpegAvPacket avpkt;
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
                "IOSVideoDecoder: Insufficient padding size");
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
    {
        qWarning() << "IOS decoder error. gotPicture=" << gotPicture << "errCode=" << res;
        // hardware decoder crash if use same frame after decoding error. It seems
        // leaves invalid ref_count on error.
        av_frame_free(&d->frame);
        d->frame = av_frame_alloc();

        if (isFatalError(res))
            d->closeCodecContext(); //< reset all

        return res; //< Negative value means error, zero means buffering.
    }

    QSize frameSize(d->frame->width, d->frame->height);
    qint64 startTimeMs = d->frame->pkt_dts / 1000;
    int frameNum = qMax(0, d->codecContext->frame_number - 1);

    QVideoFrame::PixelFormat qtPixelFormat = QVideoFrame::Format_Invalid;
    d->isHardwareAccelerated = d->frame->data[3];
    if (d->isHardwareAccelerated)
    {
        CVPixelBufferRef pixBuf = (CVPixelBufferRef)d->frame->data[3];
        qtPixelFormat = toQtPixelFormat(CVPixelBufferGetPixelFormatType(pixBuf));
    }
    else
    {
        qtPixelFormat = toQtPixelFormat((AVPixelFormat)d->frame->format);
    }
    if (qtPixelFormat == QVideoFrame::Format_Invalid)
    {
        // Recreate frame just in case.
        // I am not sure hardware decoder can reuse it.
        av_frame_free(&d->frame);
        d->frame = av_frame_alloc();
        return -1; //< report error
    }

    QAbstractVideoBuffer* buffer = new IOSMemoryBuffer(d->frame);
    d->frame = av_frame_alloc();
    QVideoFrame* videoFrame = new QVideoFrame(buffer, frameSize, qtPixelFormat);
    videoFrame->setStartTime(startTimeMs);

    outDecodedFrame->reset(videoFrame);
    return frameNum;
}

AbstractVideoDecoder::Capabilities IOSVideoDecoder::capabilities() const
{
    Q_D(const IOSVideoDecoder);
    return d->isHardwareAccelerated ? Capability::hardwareAccelerated : Capability::noCapability;
}

} // namespace media
} // namespace nx

#endif // (Q_OS_IOS)
