// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "quick_sync_video_decoder_old_player.h"

#include <nx/media/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>

#include "quick_sync_video_decoder_impl.h"
#include "quick_sync_video_frame.h"

bool QuickSyncVideoDecoderOldPlayer::isSupported(const QnConstCompressedVideoDataPtr& data)
{
    QSize size = nx::media::getFrameSize(data.get());
    if (!size.isValid())
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to check compatibility, frame size unknown");
        return false;
    }

    if (!nx::media::quick_sync::QuickSyncVideoDecoderImpl::isCompatible(
        data, data->compressionType, size.width(), size.height()))
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
        m_lastFlags = data->flags;
        if (!m_impl)
        {
            m_resolution = nx::media::getFrameSize(data.get());
            NX_DEBUG(this, "Create QuickSync video decoder: %1", m_resolution);
            m_impl = std::make_shared<nx::media::quick_sync::QuickSyncVideoDecoderImpl>();
        }
    }

    if (!m_impl)
    {
        NX_DEBUG(this, "There is no decoder to flush frames");
        return false;
    }

    CLVideoDecoderOutputPtr outFrame = *outFramePtr;
    nx::media::VideoFramePtr result;
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

    outFrame->format = AV_PIX_FMT_NV12;
    outFrame->flags = m_lastFlags | QnAbstractMediaData::MediaFlags_HWDecodingUsed;
    outFrame->pkt_dts = result->startTime();
    outFrame->width = result->size().width();
    outFrame->height = result->size().height();
    auto suraceFrame = std::make_unique<QuickSyncVideoFrame>(result, m_impl);
    outFrame->attachVideoSurface(std::move(suraceFrame));
    return true;
}

bool QuickSyncVideoDecoderOldPlayer::resetDecoder(const QnConstCompressedVideoDataPtr& /*data*/)
{
    NX_DEBUG(this, "Reset decoder");
    if (m_impl)
        m_impl->resetDecoder();
    return true;
}

int QuickSyncVideoDecoderOldPlayer::getWidth() const
{
    return m_resolution.width();
}
int QuickSyncVideoDecoderOldPlayer::getHeight() const
{
    return m_resolution.height();
}

bool QuickSyncVideoDecoderOldPlayer::hardwareDecoder() const
{
    return true;
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
