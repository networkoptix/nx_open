// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mac_video_decoder.h"
#if defined(Q_OS_DARWIN)

#if defined(TARGET_OS_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

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
#include <QtMultimedia/private/qvideotexturehelper_p.h>
#include <QtMultimedia/QAbstractVideoBuffer>

#include <nx/build_info.h>
#include <nx/codec/nal_units.h>
#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/hw_video_decoder.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/media/h264_utils.h>
#include <nx/media/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <utils/media/annexb_to_mp4.h>

#include "mac_utils.h"

namespace nx::media {

namespace {

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

    QVideoFrameFormat::PixelFormat qtPixelFormatForFrame(AVFrame* frame)
    {
        const bool hardwareAccelerated = frame->data[3];
        return hardwareAccelerated
            ? toQtPixelFormat(CVPixelBufferGetPixelFormatType((CVPixelBufferRef) frame->data[3]))
            : toQtPixelFormat((AVPixelFormat) frame->format);
    }
}

class IOSMemoryBuffer: public QAbstractVideoBuffer
{
public:
    IOSMemoryBuffer(CLVideoDecoderOutputPtr frame):
        m_frame(frame)
    {
    }

    virtual ~IOSMemoryBuffer() override
    {
    }

    virtual MapData map(QVideoFrame::MapMode /*mode*/) override
    {
        MapData data;

        data.planeCount = 1;

        if (m_frame->data[3])
        {
            // map frame to the internal buffer
            CVPixelBufferRef pixbuf = (CVPixelBufferRef) m_frame->data[3];
            CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);

            if (CVPixelBufferIsPlanar(pixbuf))
            {
                data.planeCount = CVPixelBufferGetPlaneCount(pixbuf);
                for (int i = 0; i < data.planeCount; i++)
                {
                    data.data[i] = (uint8_t*) CVPixelBufferGetBaseAddressOfPlane(pixbuf, i);
                    data.bytesPerLine[i] = CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
                    data.dataSize[i] = data.bytesPerLine[i] * CVPixelBufferGetHeightOfPlane(pixbuf, i);
                }
            }
            else
            {
                data.data[0] = (uchar*) CVPixelBufferGetBaseAddress(pixbuf);
                data.bytesPerLine[0] = CVPixelBufferGetBytesPerRow(pixbuf);
                data.dataSize[0] = data.bytesPerLine[0] * CVPixelBufferGetHeight(pixbuf);
            }
        }
        else
        {
            const auto textureDescription = QVideoTextureHelper::textureDescription(
                toQtPixelFormat((AVPixelFormat) m_frame->format));

            data.planeCount = 0;
            const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) m_frame->format);
            for (int i = 0; i < descr->nb_components && m_frame->data[i]; ++i)
            {
                ++data.planeCount;
                data.data[i] = m_frame->data[i];
                data.bytesPerLine[i] = m_frame->linesize[i];

                const size_t planeHeight = textureDescription->heightForPlane(m_frame->height, i);
                data.dataSize[i] = data.bytesPerLine[i] * planeHeight;
            }
        }

        return data;
    }

    virtual void unmap() override
    {
        CVPixelBufferRef pixbuf = (CVPixelBufferRef) m_frame->data[3];
        CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    }

    virtual QVideoFrameFormat format() const override
    {
        return QVideoFrameFormat(
            QSize(m_frame->width, m_frame->height), qtPixelFormatForFrame(m_frame.get()));
    }

private:
    CLVideoDecoderOutputPtr m_frame;
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
        outFramePtr(new CLVideoDecoderOutput()),
        lastPts(AV_NOPTS_VALUE)
    {
    }

    ~MacVideoDecoderPrivate()
    {
        closeCodecContext();
    }

    void initContext(const QnConstCompressedVideoDataPtr& frame);
    void closeCodecContext();

    std::unique_ptr<nx::media::ffmpeg::HwVideoDecoder> decoder;

    AnnexbToMp4 m_annexbToMp4;
    CLVideoDecoderOutputPtr outFramePtr;
    qint64 lastPts;
    bool isHardwareAccelerated = false;
};

void MacVideoDecoderPrivate::initContext(const QnConstCompressedVideoDataPtr& frame)
{
    if (!frame || !frame->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
        return;

    if (!nx::media::ffmpeg::HwVideoDecoder::isCompatible(
        frame->compressionType,
        QSize(frame->width, frame->height),
        /* allowOverlay */ false))
    {
        NX_WARNING(this, "Decoder is not compatible with codec %1 resolution %2x%3",
            frame->compressionType, frame->width, frame->height);
        return;
    }

    decoder = std::make_unique<nx::media::ffmpeg::HwVideoDecoder>(
        AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
        nullptr);
}

void MacVideoDecoderPrivate::closeCodecContext()
{
    decoder.reset();
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

    if (!d->decoder)
    {
        d->initContext(compressedVideoData);
        if (!d->decoder)
            return -1;
    }

    if (!d->decoder->decode(compressedVideoData, &d->outFramePtr))
    {
        const int ret = d->decoder->getLastDecodeResult(); //< Zero means buffering.
        if (ret < 0)
        {
            NX_WARNING(this, "Failed to decode frame");
            d->decoder.reset();
        }

        return ret;
    }

    d->isHardwareAccelerated = (bool) d->outFramePtr->hw_frames_ctx;

    const qint64 startTimeMs = d->outFramePtr->pkt_dts / 1000;

    if (qtPixelFormatForFrame(d->outFramePtr.get()) == QVideoFrameFormat::Format_Invalid)
    {
        NX_WARNING(this, "Unknown pixel format");
        // Recreate frame just in case.
        // I am not sure hardware decoder can reuse it.
        d->outFramePtr.reset(new CLVideoDecoderOutput());
        return -1; //< report error
    }

    auto videoFrame = new VideoFrame(std::make_unique<IOSMemoryBuffer>(d->outFramePtr));
    d->outFramePtr.reset(new CLVideoDecoderOutput());
    videoFrame->setStartTime(startTimeMs);

    outDecodedFrame->reset(videoFrame);
    return d->decoder->frameNum();
}

AbstractVideoDecoder::Capabilities MacVideoDecoder::capabilities() const
{
    Q_D(const MacVideoDecoder);
    return d->isHardwareAccelerated ? Capability::hardwareAccelerated : Capability::noCapability;
}

} // namespace nx::media

#endif // defined(Q_OS_DARWIN)
