#include "quick_sync_video_decoder.h"

#include "quick_sync_video_decoder_impl.h"

namespace nx::media {

bool QuickSyncVideoDecoder::isCompatible(
    const AVCodecID codec, const QSize& /*resolution*/, bool /*allowOverlay*/)
{
    if (codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_H265)
        return true;
    return false;
}

QSize QuickSyncVideoDecoder::maxResolution(const AVCodecID /*codec*/)
{
    return QSize(10000, 10000);
}

QuickSyncVideoDecoder::QuickSyncVideoDecoder(
    const RenderContextSynchronizerPtr& /*synchronizer*/, const QSize& /*resolution*/)
{
    m_impl = std::make_unique<QuickSyncVideoDecoderImpl>();
}

QuickSyncVideoDecoder::~QuickSyncVideoDecoder()
{
}

int QuickSyncVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result)
{
    return m_impl->decode(frame, result);
}


AbstractVideoDecoder::Capabilities QuickSyncVideoDecoder::capabilities() const
{
    return Capability::hardwareAccelerated;
}

} // namespace nx::media
