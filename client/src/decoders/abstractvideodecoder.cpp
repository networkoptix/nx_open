#include "abstractvideodecoder.h"

#include "ffmpeg/ffmpegvideodecoder_p.h"

CLAbstractVideoDecoder::CLAbstractVideoDecoder()
    : m_tryHardwareAcceleration(false),
      m_hardwareAccelerationEnabled(false),
      m_mtDecoding(false),
      m_needRecreate(false)
{
}

CLAbstractVideoDecoder::~CLAbstractVideoDecoder()
{
}


CLAbstractVideoDecoder *CLVideoDecoderFactory::createDecoder(int codecId, void *context)
{
    if (CLFFmpegVideoDecoder::isCodecSupported(codecId))
        return new CLFFmpegVideoDecoder(codecId, (AVCodecContext *)context);

    return 0;
}
