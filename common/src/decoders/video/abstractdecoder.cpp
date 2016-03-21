#include "abstractdecoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <iostream>
#include <memory>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include <utils/common/log.h>

#include <plugins/plugin_manager.h>
#include <decoders/abstractvideodecoderplugin.h>

#include "ffmpeg.h"
#include "ipp_h264_decoder.h"


CLVideoDecoderFactory::CLCodecManufacture CLVideoDecoderFactory::m_codecManufacture = AUTO;

QnAbstractVideoDecoder* CLVideoDecoderFactory::createDecoder(
    const QnCompressedVideoDataPtr& data,
    bool mtDecoding,
    const QGLContext* glContext,
    bool allowHardwareDecoding )
{
    //TODO/IMPL this is a quick solution. Need something more beautiful (static counter is not beautiful at all...)
    static QAtomicInt swDecoderCount = 0;

    // h264 
    switch( m_codecManufacture )
    {
    #ifdef Q_OS_WIN
        case INTELIPP:
            return new IPPH264Decoder();
    #endif

        case AUTO:
        {
            if( allowHardwareDecoding )
            {
                //searching for a video decoder with hardware acceleration, supporting codec type data->compressionType
                QnAbstractVideoDecoderPlugin* videoDecoderPlugin = NULL;
                const QList<QnAbstractVideoDecoderPlugin*>& plugins = PluginManager::instance()->findPlugins<QnAbstractVideoDecoderPlugin>();
                for( QnAbstractVideoDecoderPlugin* plugin: plugins )
                {
                    if( !plugin->isHardwareAccelerated() || plugin->supportedCodecTypes().indexOf(data->compressionType) == -1 )
                        continue;
                    videoDecoderPlugin = plugin;
                    break;
                }
                if( videoDecoderPlugin )
                {
                    std::auto_ptr<QnAbstractVideoDecoder> decoder( videoDecoderPlugin->create( data->compressionType, data, glContext, swDecoderCount.load() ) );
                    if( decoder.get() && decoder->isHardwareAccelerationEnabled() )
                        return decoder.release();
                }
                NX_LOG( lit("Hardware acceleration is not supported. Switching to software decoding..."), cl_logWARNING );
            }
        }

        case FFMPEG:
        default:
        {
            return new CLFFmpegVideoDecoder( data->compressionType, data, mtDecoding, &swDecoderCount );
        }
    }

    return NULL;
}

void QnAbstractVideoDecoder::setTryHardwareAcceleration( bool tryHardwareAcceleration )
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

#endif // ENABLE_DATA_PROVIDERS
