// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mac_video_decoder.h"
#if defined(Q_OS_IOS) || defined(Q_OS_MACOS)

extern "C"
{
#include <libavcodec/videotoolbox.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
} // extern "C"

#include <QtCore/QDebug>
#include <QtCore/QFile>

#include <nx/build_info.h>
#include <nx/codec/nal_units.h>
#include <nx/media/ffmpeg/old_api.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/media/h264_utils.h>
#include <nx/media/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <utils/media/annexb_to_mp4.h>

#include "aligned_mem_video_buffer.h"
#include "mac_utils.h"

#if defined(TARGET_OS_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

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

    QVideoFrameFormat::PixelFormat toQtPixelFormat(OSType pixFormat)
    {
        switch (pixFormat)
        {
            case kCVPixelFormatType_420YpCbCr8Planar:
                return QVideoFrameFormat::Format_YUV420P;
            case kCVPixelFormatType_422YpCbCr8:
                return QVideoFrameFormat::Format_YV12;
            case kCVPixelFormatType_32BGRA:
                return QVideoFrameFormat::Format_BGRA8888;
            case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
            case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
                return QVideoFrameFormat::Format_NV12;
            default:
                return QVideoFrameFormat::Format_Invalid;
        }
    }
}

class IOSMemoryBuffer: public QAbstractVideoBuffer
{
public:
    IOSMemoryBuffer(AVFrame* frame):
        QAbstractVideoBuffer(QVideoFrame::NoHandle),
        m_frame(frame)
    {
    }

    ~IOSMemoryBuffer()
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

        m_mapMode = mode;

        data.nPlanes = 1;

        if (m_frame->data[3])
        {
            // map frame to the internal buffer
            CVPixelBufferRef pixbuf = (CVPixelBufferRef) m_frame->data[3];
            CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);

            if (CVPixelBufferIsPlanar(pixbuf))
            {
                data.nPlanes = CVPixelBufferGetPlaneCount(pixbuf);
                for (int i = 0; i < data.nPlanes; i++)
                {
                    data.data[i] = (uint8_t*) CVPixelBufferGetBaseAddressOfPlane(pixbuf, i);
                    data.bytesPerLine[i] = CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
                    data.size[i] = data.bytesPerLine[i] * CVPixelBufferGetHeightOfPlane(pixbuf, i);
                }
            }
            else
            {
                data.data[0] = (uchar*) CVPixelBufferGetBaseAddress(pixbuf);
                data.bytesPerLine[0] = CVPixelBufferGetBytesPerRow(pixbuf);
                data.size[0] = data.bytesPerLine[0] * CVPixelBufferGetHeight(pixbuf);
            }
        }
        else
        {
            // map frame to the ffmpeg buffer
            data.nPlanes = 0;
            const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) m_frame->format);
            for (int i = 0; i < descr->nb_components && m_frame->data[i]; ++i)
            {
                ++data.nPlanes;
                data.data[i] = m_frame->data[i];
                data.bytesPerLine[i] = m_frame->linesize[i];

                int bytesPerPlane = data.bytesPerLine[i] * m_frame->height;
                if (i > 0)
                {
                    bytesPerPlane >>= descr->log2_chroma_h + descr->log2_chroma_w;
                    bytesPerPlane *= descr->comp->step;
                }
                data.size[i] = bytesPerPlane;
            }
        }

        return data;
    }

    virtual void unmap() override
    {
        CVPixelBufferRef pixbuf = (CVPixelBufferRef) m_frame->data[3];
        CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    }

private:
    AVFrame* m_frame = nullptr;
    QVideoFrame::MapMode m_mapMode = QVideoFrame::NotMapped;
};

//-------------------------------------------------------------------------------------------------
// FfmpegDecoderPrivate

class MacVideoDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(MacVideoDecoder)
    MacVideoDecoder* q_ptr;

public:
    MacVideoDecoderPrivate()
    :
        codecContext(nullptr),
        frame(av_frame_alloc()),
        lastPts(AV_NOPTS_VALUE)
    {
    }

    ~MacVideoDecoderPrivate()
    {
        closeCodecContext();
        av_frame_free(&frame);
    }

    void initContext(const QnConstCompressedVideoDataPtr& frame);
    void closeCodecContext();

    AnnexbToMp4 m_annexbToMp4;
    AVCodecContext* codecContext;
    AVFrame* frame;
    qint64 lastPts;
    bool isHardwareAccelerated = false;
};

void MacVideoDecoderPrivate::initContext(const QnConstCompressedVideoDataPtr& frame)
{
    if (!frame || !frame->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
        return;

    auto codec = avcodec_find_decoder(frame->compressionType);
    codecContext = avcodec_alloc_context3(codec);
    if (frame->context)
        frame->context->toAvCodecContext(codecContext);

    QSize frameSize = QSize(codecContext->width, codecContext->height);
    if (!isValidFrameSize(frameSize))
    {
        frameSize = getFrameSize(frame.get());
        codecContext->width = frameSize.width();
        codecContext->height = frameSize.height();
    }
    if (!isValidFrameSize(frameSize))
    {
        closeCodecContext();
        return;
    }

    codecContext->thread_count = 1;
    codecContext->opaque = this;

    if (avcodec_open2(codecContext, codec, nullptr) < 0)
    {
        NX_WARNING(this, "Can't open decoder for codec %1 resolution %2x%3",
            frame->compressionType, codecContext->width, codecContext->height);
        closeCodecContext();
        return;
    }
}

void MacVideoDecoderPrivate::closeCodecContext()
{
    avcodec_free_context(&codecContext);
}

//-------------------------------------------------------------------------------------------------
// FfmpegDecoder

MacVideoDecoder::MacVideoDecoder(
    const RenderContextSynchronizerPtr& /*synchronizer*/,
    const QSize& /*resolution*/)
    :
    AbstractVideoDecoder(),
    d_ptr(new MacVideoDecoderPrivate())
{
}

MacVideoDecoder::~MacVideoDecoder()
{
}

bool MacVideoDecoder::isCompatible(
    const AVCodecID codec,
    const QSize& resolution,
    bool /*allowOverlay*/,
    bool allowHardwareAcceleration)
{
    if (!allowHardwareAcceleration)
        return false;

    if (!mac_isHWDecodingSupported(codec))
        return false;

    const QSize maxRes = maxResolution(codec);
    const auto fixedResolution = resolution.height() > resolution.width()
        ? resolution.transposed()
        : resolution;
    return fixedResolution.width() <= maxRes.width() && fixedResolution.height() <= maxRes.height();
}

QSize MacVideoDecoder::maxResolution(const AVCodecID codec)
{
    static const QSize kFullHdResolution(1920, 1080);

    static QSize maxH264Resolution = mac_maxDecodeResolutionH264();
    static QSize maxHevcResolution = mac_maxDecodeResolutionHevc();

    switch (codec)
    {
        case AV_CODEC_ID_H264:
            return maxH264Resolution;
        case AV_CODEC_ID_H265:
            return maxHevcResolution;
        // TODO: If AV1 or VP9 decoding is supported we should enable higher resolutions.
        default:
            return kFullHdResolution;
    }
}

int MacVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& videoData,
    VideoFramePtr* outDecodedFrame)
{
    Q_D(MacVideoDecoder);

    auto compressedVideoData = videoData;
    if (videoData && nx::media::isAnnexb(videoData.get()))
    {
        compressedVideoData = d->m_annexbToMp4.process(videoData.get());
        if (!compressedVideoData)
        {
            NX_WARNING(this, "Decoding failed, cannot convert to MP4 format");
            return -1;
        }
    }

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
    int res = nx::media::ffmpeg::old_api::decode(d->codecContext, d->frame, &gotPicture, &avpkt);
    if (res <= 0 || !gotPicture)
    {
        NX_WARNING(this, "IOS decoder error. gotPicture %1, errorCode %2", gotPicture, res);
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
    int frameNum = qMax(0, d->codecContext->frame_num - 1);

    QVideoFrameFormat::PixelFormat qtPixelFormat = QVideoFrameFormat::Format_Invalid;
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
    if (qtPixelFormat == QVideoFrameFormat::Format_Invalid)
    {
        NX_WARNING(this, "Unknown pixel format");
        // Recreate frame just in case.
        // I am not sure hardware decoder can reuse it.
        av_frame_free(&d->frame);
        d->frame = av_frame_alloc();
        return -1; //< report error
    }

    QAbstractVideoBuffer* buffer = new IOSMemoryBuffer(d->frame);
    d->frame = av_frame_alloc();
    VideoFrame* videoFrame = new VideoFrame(buffer, {frameSize, qtPixelFormat});
    videoFrame->setStartTime(startTimeMs);

    outDecodedFrame->reset(videoFrame);
    return frameNum;
}

AbstractVideoDecoder::Capabilities MacVideoDecoder::capabilities() const
{
    Q_D(const MacVideoDecoder);
    return d->isHardwareAccelerated ? Capability::hardwareAccelerated : Capability::noCapability;
}

} // namespace media
} // namespace nx

#endif // (Q_OS_IOS)
