// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hw_video_decoder_old_player.h"

#include <QtGui/rhi/qrhi.h>

#include <nx/build_info.h>
#include <nx/media/ffmpeg/hw_video_decoder.h>
#include <nx/media/media_fwd.h>
#include <nx/media/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
} // extern "C"

#include "hw_video_api.h"
#include "texture_helper.h"

namespace nx::media {

namespace {

AVHWDeviceType deviceTypeFromRhi(QRhi* rhi)
{
    switch (rhi->backend())
    {
        case QRhi::Implementation::D3D11:
            return AV_HWDEVICE_TYPE_D3D11VA;
        case QRhi::Implementation::Metal:
            return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
        case QRhi::Vulkan:
            return AV_HWDEVICE_TYPE_VULKAN;
        case QRhi::Implementation::OpenGLES2:
            if (nx::build_info::isLinux())
                return AV_HWDEVICE_TYPE_VAAPI;
            [[fallthrough]];
        default:
            return AV_HWDEVICE_TYPE_NONE;
    }
}

class NX_MEDIA_API VideoFrameAdapter: public AbstractVideoSurface
{
public:
    VideoFrameAdapter(
        const nx::media::VideoFramePtr& frame,
        std::weak_ptr<nx::media::ffmpeg::HwVideoDecoder> decoder)
        :
        m_frame(frame),
        m_decoder(decoder)
    {
    }

    ~VideoFrameAdapter()
    {
    }

    virtual AVFrame lockFrame() override
    {
        AVFrame result;
        memset(&result, 0, sizeof(AVFrame));
        return result;
    }

    virtual void unlockFrame() override
    {
    }

    virtual SurfaceType type() override { return SurfaceType::VideoFrame; }

    virtual QVideoFrame* frame() override { return m_frame.get(); }

private:
    nx::media::VideoFramePtr m_frame;
    std::weak_ptr<nx::media::ffmpeg::HwVideoDecoder> m_decoder;
};

} // namespace

bool HwVideoDecoderOldPlayer::isSupported(const QnConstCompressedVideoDataPtr& data)
{
    QSize size = nx::media::getFrameSize(data.get());
    if (!size.isValid())
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to check compatibility, frame size unknown");
        return false;
    }

    if (!nx::media::ffmpeg::HwVideoDecoder::isCompatible(
        data->compressionType,
        size,
        /* allowOverlay */ false))
    {
        return false;
    }

    if (!data->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
    {
        NX_ERROR(NX_SCOPE_TAG,
            "Failed to check FFmpeg HW decoder compatibility, input frame is not a key frame");
        return false;
    }

    return true;
}

int HwVideoDecoderOldPlayer::m_instanceCount = 0;

int HwVideoDecoderOldPlayer::instanceCount()
{
    return m_instanceCount;
}

HwVideoDecoderOldPlayer::HwVideoDecoderOldPlayer(QRhi* rhi):
    m_rhi(rhi)
{
    ++m_instanceCount;
}

HwVideoDecoderOldPlayer::~HwVideoDecoderOldPlayer()
{
    --m_instanceCount;
}

bool HwVideoDecoderOldPlayer::decode(
    const QnConstCompressedVideoDataPtr& data, CLVideoDecoderOutputPtr* const outFramePtr)
{
    if (!outFramePtr || !outFramePtr->data())
    {
        NX_ERROR(this, "Empty output frame provided");
        return false;
    }

    if (!m_rhi || m_rhi->isDeviceLost())
    {
        NX_WARNING(this, m_rhi ? "RHI device lost" : "No RHI device");
        m_rhi = nullptr;
        return false;
    }

    if (data)
    {
        m_lastFlags = data->flags;
        if (!m_impl)
        {
            m_resolution = nx::media::getFrameSize(data.get());

            NX_DEBUG(this, "Create FFmpeg HW video decoder: %1", m_resolution);

            AVHWDeviceType hwType = deviceTypeFromRhi(m_rhi);
            auto api = VideoApiRegistry::instance()->get(hwType);

            std::unique_ptr<nx::media::ffmpeg::AvOptions> options;
            nx::media::ffmpeg::HwVideoDecoder::InitFunc initFunc;

            if (m_lastStatus != 0 || !m_isHW || !api)
            {
                NX_DEBUG(this, "Switching to FFmpeg SW video decoder, status: %1", m_lastStatus);
                hwType = AV_HWDEVICE_TYPE_NONE;
                initFunc = {};
            }
            else
            {
                initFunc = api->initFunc(m_rhi);
                options = api->options(m_rhi);
            }

            m_impl = std::make_shared<nx::media::ffmpeg::HwVideoDecoder>(
                hwType,
                /* metrics */ nullptr,
                std::move(options),
                initFunc);
        }
    }

    if (!m_impl)
    {
        NX_DEBUG(this, "There is no decoder to flush frames");
        return false;
    }

    if (!m_impl->decode(data, outFramePtr))
    {
        m_lastStatus = m_impl->getLastDecodeResult();
        if (m_lastStatus < 0)
        {
            NX_WARNING(this, "Failed to decode frame");
            m_impl.reset();
        }
        return false;
    }

    m_lastStatus = 0;

    if ((*outFramePtr)->hw_frames_ctx)
    {
        const auto frame = (*outFramePtr).get();

        auto api = VideoApiRegistry::instance()->get(frame);
        const auto pixelFormat = nx::media::toQtPixelFormat(frame);
        if (!api || (pixelFormat == QVideoFrameFormat::Format_Invalid && m_isHW))
        {
            NX_WARNING(this, "HW pixel format %1 is not supported", getFormat(frame));
            m_impl.reset();
            m_isHW = false;
            return decode(data, outFramePtr);
        }

        nx::media::VideoFramePtr result = api->makeFrame(frame);

        if (!result)
        {
            NX_WARNING(this, "Failed to wrap ffmpeg frame as QVideoFrame");
            return false;
        }

        (*outFramePtr)->flags = m_lastFlags | QnAbstractMediaData::MediaFlags_HWDecodingUsed;
        auto suraceFrame = std::make_unique<VideoFrameAdapter>(result, m_impl);
        (*outFramePtr)->attachVideoSurface(std::move(suraceFrame));
    }
    return true;
}

bool HwVideoDecoderOldPlayer::resetDecoder(const QnConstCompressedVideoDataPtr& /*data*/)
{
    NX_DEBUG(this, "Reset decoder");
    m_impl.reset();
    return true;
}

int HwVideoDecoderOldPlayer::getWidth() const
{
    return m_resolution.width();
}

int HwVideoDecoderOldPlayer::getHeight() const
{
    return m_resolution.height();
}

bool HwVideoDecoderOldPlayer::hardwareDecoder() const
{
    return true;
}

double HwVideoDecoderOldPlayer::getSampleAspectRatio() const
{
    // TODO
    return 1.0;
}

void HwVideoDecoderOldPlayer::setLightCpuMode(DecodeMode /*val*/)
{
    // TODO
}

void HwVideoDecoderOldPlayer::setMultiThreadDecodePolicy(
    MultiThreadDecodePolicy /*mtDecodingPolicy*/)
{
}

} // namespace nx::media
