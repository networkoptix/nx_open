// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_video_decoder_old_player.h"

#include <utils/media/utils.h>
#include <nx/utils/log/log.h>
#include <nx/media/nvidia/nvidia_video_frame.h>

#include "nvidia_video_decoder.h"

bool NvidiaVideoDecoderOldPlayer::isSupported(const QnConstCompressedVideoDataPtr& data)
{
  /*  QSize size = nx::media::getFrameSize(data.get());
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
*/
    return true;
}

int NvidiaVideoDecoderOldPlayer::m_instanceCount = 0;

int NvidiaVideoDecoderOldPlayer::instanceCount()
{
    return m_instanceCount;
}

NvidiaVideoDecoderOldPlayer::NvidiaVideoDecoderOldPlayer()
{
    m_instanceCount++;
}

NvidiaVideoDecoderOldPlayer::~NvidiaVideoDecoderOldPlayer()
{
    m_instanceCount--;
}

bool NvidiaVideoDecoderOldPlayer::decode(
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
            m_impl = std::make_shared<nx::media::nvidia::NvidiaVideoDecoder>();
        }
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
    //if (!result)
     //   return false;

    outFrame->format = AV_PIX_FMT_NV12;
    outFrame->flags = m_lastFlags | QnAbstractMediaData::MediaFlags_HWDecodingUsed;
    auto frame = m_impl->getFrame();
    if (!frame)
        return false;
    outFrame->pkt_dts = frame->timestamp;
    outFrame->width = frame->width;
    outFrame->height = frame->height;
    NX_INFO(this, "QQQ decoded new frame: %1", frame->timestamp);
    outFrame->attachVideoSurface(std::move(frame));
    return true;
}

bool NvidiaVideoDecoderOldPlayer::resetDecoder(const QnConstCompressedVideoDataPtr& /*data*/)
{
    NX_DEBUG(this, "Reset decoder");
    if (m_impl)
        m_impl->resetDecoder();
    return true;
}

int NvidiaVideoDecoderOldPlayer::getWidth() const
{
    return m_resolution.width();
}
int NvidiaVideoDecoderOldPlayer::getHeight() const
{
    return m_resolution.height();
}

MemoryType NvidiaVideoDecoderOldPlayer::targetMemoryType() const
{
    return MemoryType::VideoMemory;
}

double NvidiaVideoDecoderOldPlayer::getSampleAspectRatio() const
{
    // TODO
    return 1.0;
}

void NvidiaVideoDecoderOldPlayer::setLightCpuMode(DecodeMode /*val*/)
{
    // TODO
}

void NvidiaVideoDecoderOldPlayer::setMultiThreadDecodePolicy(
    MultiThreadDecodePolicy /*mtDecodingPolicy*/)
{

}
