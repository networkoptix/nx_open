#include "quick_sync_video_decoder_old_player.h"

#include <utils/media/utils.h>
#include <nx/utils/log/log.h>

#include "quick_sync_video_decoder_impl.h"
#include "quick_sync_video_frame.h"

bool QuickSyncVideoDecoderOldPlayer::isSupported(const QnConstCompressedVideoDataPtr& data)
{
    QSize size = nx::media::getFrameSize(data, true);
    if (!size.isValid())
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to check compatibility, frame size unknown");
        return false;
    }

    if (!nx::media::quick_sync::QuickSyncVideoDecoderImpl::isCompatible(
        data->compressionType, size.width(), size.height()))
    {
        return false;
    }

    if (!data->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
    {
        NX_ERROR(NX_SCOPE_TAG,
            "Failed to check QuickSync compatibility, input frame is not a key frame");
        return false;
    }

    return true;
}

int QuickSyncVideoDecoderOldPlayer::m_instanceCount = 0;

int QuickSyncVideoDecoderOldPlayer::instanceCount()
{
    return m_instanceCount;
}

QuickSyncVideoDecoderOldPlayer::QuickSyncVideoDecoderOldPlayer()
{
    m_instanceCount++;
}

QuickSyncVideoDecoderOldPlayer::~QuickSyncVideoDecoderOldPlayer()
{
    m_instanceCount--;
}

bool QuickSyncVideoDecoderOldPlayer::decode(
    const QnConstCompressedVideoDataPtr& data, CLVideoDecoderOutputPtr* const outFramePtr)
{
    if (!outFramePtr || !outFramePtr->data())
    {
        NX_ERROR(this, "Empty output frame provided");
        return false;
    }

    if (data)
    {
        // reset decoder on new stream params
        auto frameResolution = nx::media::getFrameSize(data);
        bool resolutionChanged = !frameResolution.isEmpty() && m_resolution != frameResolution;
        if (!m_impl || m_codecId != data->compressionType || resolutionChanged)
        {
            NX_DEBUG(this, "Create QuickSync video decoder");
            m_resolution = frameResolution;
            m_impl = std::make_shared<nx::media::quick_sync::QuickSyncVideoDecoderImpl>();
        }
        m_codecId = data->compressionType;
        if (!frameResolution.isEmpty())
            m_resolution = frameResolution;
    }

    if (!m_impl)
    {
        NX_DEBUG(this, "There is no decoder to flush frames");
        return false;
    }

    CLVideoDecoderOutputPtr outFrame = *outFramePtr;
    nx::QVideoFramePtr result;
    if (m_impl->decode(data, &result) < 0)
    {
        NX_WARNING(this, "Failed to decode frame");
        m_impl.reset();
        m_lastStatus = -1;
        return false;
    }
    m_lastStatus = 0;
    if (!result)
        return false;

    std::weak_ptr<nx::media::quick_sync::QuickSyncVideoDecoderImpl> decoderWeakPtr = m_impl;
    outFrame->flags |= QnAbstractMediaData::MediaFlags_HWDecodingUsed;
    outFrame->pkt_dts = result->startTime();
    outFrame->width = result->size().width();
    outFrame->height = result->size().height();
    auto suraceFrame = std::make_unique<QuickSyncVideoFrame>(result);
    outFrame->attachVideoSurface(std::move(suraceFrame));
    return true;
}

void QuickSyncVideoDecoderOldPlayer::resetDecoder(const QnConstCompressedVideoDataPtr& /*data*/)
{
    NX_DEBUG(this, "Reset decoder");
    m_impl.reset();
}

void QuickSyncVideoDecoderOldPlayer::resetDecoder()
{
    NX_DEBUG(this, "Reset decoder");
    m_impl->resetDecoder();
}

int QuickSyncVideoDecoderOldPlayer::getWidth() const
{
    return m_resolution.width();
}
int QuickSyncVideoDecoderOldPlayer::getHeight() const
{
    return m_resolution.height();
}

AVPixelFormat QuickSyncVideoDecoderOldPlayer::GetPixelFormat() const
{
    return AV_PIX_FMT_NV12;
}

MemoryType QuickSyncVideoDecoderOldPlayer::targetMemoryType() const
{
    return MemoryType::VideoMemory;
}

double QuickSyncVideoDecoderOldPlayer::getSampleAspectRatio() const
{
    // TODO
    return 1.0;
}

void QuickSyncVideoDecoderOldPlayer::setLightCpuMode(DecodeMode /*val*/)
{
    // TODO
}

void QuickSyncVideoDecoderOldPlayer::setMultiThreadDecodePolicy(
    MultiThreadDecodePolicy /*mtDecodingPolicy*/)
{

}
