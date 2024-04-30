// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_video_decoder_old_player.h"

#include <nx/media/nvidia/nvidia_video_frame.h>
#include <nx/media/utils.h>
#include <nx/utils/log/log.h>

#include "nvidia_video_decoder.h"

int NvidiaVideoDecoderOldPlayer::m_instanceCount = 0;

bool NvidiaVideoDecoderOldPlayer::isSupported(const QnConstCompressedVideoDataPtr& data)
{
    return nx::media::nvidia::NvidiaVideoDecoder::isCompatible(
        data, data->compressionType, data->height, data->width);
}

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
            NX_DEBUG(this, "Create Nvidia video decoder: %1", m_resolution);
            m_impl = std::make_shared<nx::media::nvidia::NvidiaVideoDecoder>();
        }
    }

    if (!m_impl)
    {
        NX_DEBUG(this, "There is no decoder to flush frames");
        return false;
    }

    CLVideoDecoderOutputPtr outFrame = *outFramePtr;
    if (!m_impl->decode(data))
    {
        NX_WARNING(this, "Failed to decode frame");
        m_impl.reset();
        m_lastStatus = -1;
        return false;
    }
    m_lastStatus = 0;

    outFrame->format = AV_PIX_FMT_NV12;
    outFrame->flags = m_lastFlags | QnAbstractMediaData::MediaFlags_HWDecodingUsed;
    auto frame = m_impl->getFrame();
    if (!frame)
        return false;
    outFrame->pkt_dts = frame->timestamp;
    outFrame->width = frame->width;
    outFrame->height = frame->height;
    outFrame->attachVideoSurface(std::move(frame));
    return true;
}

bool NvidiaVideoDecoderOldPlayer::resetDecoder(const QnConstCompressedVideoDataPtr& /*data*/)
{
    NX_DEBUG(this, "Reset decoder");
    if (m_impl)
        m_impl.reset();
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

bool NvidiaVideoDecoderOldPlayer::hardwareDecoder() const
{
    return true;
}

double NvidiaVideoDecoderOldPlayer::getSampleAspectRatio() const
{
    //TODO #lbusygin: Implement sample acpect ratio parsing
    return 1.0;
}

void NvidiaVideoDecoderOldPlayer::setLightCpuMode(DecodeMode /*val*/)
{
    //TODO #lbusygin: Implement light CPU mode for nvidia
}

void NvidiaVideoDecoderOldPlayer::setMultiThreadDecodePolicy(
    MultiThreadDecodePolicy /*mtDecodingPolicy*/)
{
}
