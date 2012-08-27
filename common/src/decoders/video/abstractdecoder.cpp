
#include "abstractdecoder.h"

#include <iostream>

#include <libavcodec/avcodec.h>

#include "ffmpeg.h"
#include "ipp_h264_decoder.h"
#include "xvbadecoder.h"


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
#endif

    case FFMPEG:
    default:
        {
            if( data->compressionType == CODEC_ID_H264 )
            {
                QnAbstractVideoDecoder* decoder = new QnXVBADecoder(data);
                if( decoder->isHardwareAccelerationEnabled() )
                    return decoder;
                std::cout<<"mark22\n";
                delete decoder;
                cl_log.log( QString::fromAscii("XVBA acceleration is not supported. Switching to software decoding..."), cl_logWARNING );
            }
            return new CLFFmpegVideoDecoder( data->compressionType, data, mtDecoding );
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
