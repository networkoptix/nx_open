#include "quick_sync_video_decoder.h"

#include "quick_sync_video_decoder_impl.h"

namespace nx::media::quick_sync {

bool QuickSyncVideoDecoder::isCompatible(
    const AVCodecID codec, const QSize& resolution, bool /*allowOverlay*/)
{
    return QuickSyncVideoDecoderImpl::isCompatible(codec, resolution.width(), resolution.height());
}

QSize QuickSyncVideoDecoder::maxResolution(const AVCodecID /*codec*/)
{
    return QSize(8192, 8192);
}

QuickSyncVideoDecoder::QuickSyncVideoDecoder(
    const RenderContextSynchronizerPtr& /*synchronizer*/, const QSize& /*resolution*/)
{
    m_impl = std::make_shared<QuickSyncVideoDecoderImpl>();
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

} // namespace nx::media::quick_sync
