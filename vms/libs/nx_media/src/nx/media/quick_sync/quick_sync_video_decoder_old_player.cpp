#include "quick_sync_video_decoder_old_player.h"
#include "quick_sync_video_decoder_impl.h"
#include "va_display.h"

#include <utils/media/utils.h>

bool QuickSyncVideoDecoder::isSupported()
{
    return VaDisplay::getDisplay() != nullptr;
}

bool QuickSyncVideoDecoder::decode(
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
            m_impl = std::make_shared<nx::media::QuickSyncVideoDecoderImpl>();
        }
        m_codecId = data->compressionType;
        if (!frameResolution.isEmpty())
            m_resolution = frameResolution;
    }

    CLVideoDecoderOutputPtr outFrame = *outFramePtr;
    nx::QVideoFramePtr result;
    if (m_impl->decode(data, &result) < 0)
    {
        NX_ERROR(this, "Failed to decode frame");
        m_impl.reset();
        return false;
    }
    if (!result)
        return false;

    std::weak_ptr<nx::media::QuickSyncVideoDecoderImpl> decoderWeakPtr = m_impl;
    outFrame->attachToVideoMemory(result, std::move(decoderWeakPtr));
    return true;
}

const AVFrame* QuickSyncVideoDecoder::lastFrame() const
{
    // TODO
    return nullptr;
}

void QuickSyncVideoDecoder::resetDecoder(const QnConstCompressedVideoDataPtr& /*data*/)
{
    NX_DEBUG(this, "Reset decoder");
    m_impl.reset();
}

int QuickSyncVideoDecoder::getWidth() const
{
    return m_resolution.width();
}
int QuickSyncVideoDecoder::getHeight() const
{
    return m_resolution.height();
}

AVPixelFormat QuickSyncVideoDecoder::GetPixelFormat() const
{
    return AV_PIX_FMT_NV12;
}

MemoryType QuickSyncVideoDecoder::targetMemoryType() const
{
    return MemoryType::VideoMemory;
}

double QuickSyncVideoDecoder::getSampleAspectRatio() const
{
    // TODO
    return 1.0;
}

void QuickSyncVideoDecoder::setLightCpuMode(DecodeMode /*val*/)
{
    // TODO
}

void QuickSyncVideoDecoder::setMultiThreadDecodePolicy(MultiThreadDecodePolicy /*mtDecodingPolicy*/)
{}
