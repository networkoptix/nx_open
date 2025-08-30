// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "quick_sync_video_decoder.h"

#include "quick_sync_video_decoder_impl.h"

namespace nx::media::quick_sync {

bool QuickSyncVideoDecoder::isCompatible(
    const AVCodecID codec,
    const QSize& resolution,
    bool /*allowHardwareAcceleration*/)
{
    return QuickSyncVideoDecoderImpl::isCompatible(
        nullptr, codec, resolution.width(), resolution.height());
}

bool QuickSyncVideoDecoder::isAvailable()
{
    return QuickSyncVideoDecoderImpl::isAvailable();
}

QSize QuickSyncVideoDecoder::maxResolution(const AVCodecID /*codec*/)
{
    return QSize(8192, 8192);
}

QuickSyncVideoDecoder::QuickSyncVideoDecoder(const QSize& /*resolution*/, QRhi*)
{
    m_impl = std::make_shared<QuickSyncVideoDecoderImpl>();
}

QuickSyncVideoDecoder::~QuickSyncVideoDecoder()
{
}

int QuickSyncVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& frame, VideoFramePtr* result)
{
    return m_impl->decode(frame, result);
}

bool QuickSyncVideoDecoder::sendPacket(const QnConstCompressedVideoDataPtr& packet)
{
    m_frameNumber = m_impl->decode(packet, &m_result);
    return m_frameNumber > 0;
}

bool QuickSyncVideoDecoder::receiveFrame(VideoFramePtr* decodedFrame)
{
    if (m_result)
        *decodedFrame = m_result;
    m_result.reset();
    return true;
}

int QuickSyncVideoDecoder::currentFrameNumber() const
{
    return m_frameNumber > 0 ? m_frameNumber : 0;
}

AbstractVideoDecoder::Capabilities QuickSyncVideoDecoder::capabilities() const
{
    return Capability::hardwareAccelerated;
}

} // namespace nx::media::quick_sync
