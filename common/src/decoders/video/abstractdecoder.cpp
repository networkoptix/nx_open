
#include "abstractdecoder.h"
#include "ffmpeg.h"
#include "quicksyncvideodecoder.h"
#include "ipp_h264_decoder.h"
#ifdef Q_OS_WIN
#include "quicksyncvideodecoder.h"
#endif



CLVideoDecoderFactory::CLCodecManufacture CLVideoDecoderFactory::m_codecManufacture = FFMPEG;

QnAbstractVideoDecoder* CLVideoDecoderFactory::createDecoder( const QnCompressedVideoDataPtr data, bool mtDecoding )
{
    // h264 
    switch(m_codecManufacture)
    {
#ifdef Q_OS_WIN
    case INTELIPP:
        return new IPPH264Decoder();
        break;

#ifndef _DEBUG
    case INTEL_QUICK_SYNC:
    {
        QuickSyncVideoDecoder* decoder = new QuickSyncVideoDecoder();
        decoder->decode( data, NULL );
        return decoder;
    }
#endif
#endif

    case FFMPEG:
    default:
#if defined(_WIN32) && !defined(_DEBUG)
        return new QuickSyncVideoDecoder();
#else
        return new CLFFmpegVideoDecoder(data->compressionType, data, mtDecoding);
#endif
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
