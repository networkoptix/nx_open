
#include "abstractdecoder.h"
#include "ffmpeg.h"
#include "ipp_h264_decoder.h"
#include "quicksyncvideodecoder.h"


CLVideoDecoderFactory::CLCodecManufacture CLVideoDecoderFactory::m_codecManufacture = FFMPEG;

QnAbstractVideoDecoder* CLVideoDecoderFactory::createDecoder(const QnCompressedVideoDataPtr data, bool mtDecoding)
{
    // h264 
    switch(m_codecManufacture)
    {
#ifdef Q_OS_WIN
    case INTELIPP:
        return new IPPH264Decoder();
        break;
#endif
    case FFMPEG:
    default:
        {
#ifdef _DEBUG
            QuickSyncVideoDecoder* decoder = new QuickSyncVideoDecoder();
            decoder->decode( data, NULL );
            return decoder;
#else
        return new CLFFmpegVideoDecoder(data->compressionType, data, mtDecoding);
#endif
        }
    }

    return 0;
}

void QnAbstractVideoDecoder::setTryHardwareAcceleration(bool tryHardwareAcceleration)
{
    m_tryHardwareAcceleration = tryHardwareAcceleration;
}

QnAbstractVideoDecoder::QnAbstractVideoDecoder()
:
    m_tryHardwareAcceleration(false),
    m_hardwareAccelerationEnabled(false),
    m_mtDecoding(false),
    m_needRecreate(false)
{
}

bool QnAbstractVideoDecoder::isHardwareAccelerationEnabled() const 
{
    return m_hardwareAccelerationEnabled;
}
