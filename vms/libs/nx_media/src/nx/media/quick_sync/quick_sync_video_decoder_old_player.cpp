#include "quick_sync_video_decoder_old_player.h"
#include "quick_sync_video_decoder_impl.h"

QuickSyncVideoDecoder::QuickSyncVideoDecoder()
{
    m_impl = std::make_unique<nx::media::QuickSyncVideoDecoderImpl>();
}

bool QuickSyncVideoDecoder::decode( const QnConstCompressedVideoDataPtr& /*data*/, QSharedPointer<CLVideoDecoderOutput>* const /*outFrame*/)
{
    return true;
}

const AVFrame* QuickSyncVideoDecoder::lastFrame() const
{
    return nullptr;
}

void QuickSyncVideoDecoder::resetDecoder(const QnConstCompressedVideoDataPtr& /*data*/)
{
}

int QuickSyncVideoDecoder::getWidth() const
{
    return m_resulution.width();
}
int QuickSyncVideoDecoder::getHeight() const
{
    return m_resulution.height();
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
    return 1.0;
}

void QuickSyncVideoDecoder::setSpeed(float /*newValue*/)
{

}
void QuickSyncVideoDecoder::setLightCpuMode(DecodeMode /*val*/)
{
}

void QuickSyncVideoDecoder::setMultiThreadDecodePolicy(MultiThreadDecodePolicy /*mtDecodingPolicy*/)
{
}
