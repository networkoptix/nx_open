#include "abstract_video_decoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <iostream>
#include <memory>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include <nx/utils/log/log.h>

#include <plugins/plugin_manager.h>
#include <decoders/abstractvideodecoderplugin.h>

#include "ffmpeg_video_decoder.h"
#include "ipp_h264_decoder.h"


QnVideoDecoderFactory::CLCodecManufacture QnVideoDecoderFactory::m_codecManufacture = AUTO;
QnCommonModule* QnVideoDecoderFactory::m_commonModule = nullptr;

QnAbstractVideoDecoder* QnVideoDecoderFactory::createDecoder(
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
                NX_ASSERT(m_commonModule, lit("Common module should be set"));
                if (!m_commonModule)
                    return nullptr;

                auto pluginManager = m_commonModule->pluginManager();
                const auto& plugins = pluginManager->findPlugins<QnAbstractVideoDecoderPlugin>();
                for( QnAbstractVideoDecoderPlugin* plugin: plugins )
                {
                    if( !plugin->isHardwareAccelerated() || plugin->supportedCodecTypes().indexOf(data->compressionType) == -1 )
                        continue;
                    videoDecoderPlugin = plugin;
                    break;
                }
                if( videoDecoderPlugin )
                {
                    std::unique_ptr<QnAbstractVideoDecoder> decoder(
                        videoDecoderPlugin->create(
                            data->compressionType, data, glContext, swDecoderCount.load()));

                    if( decoder.get() && decoder->isHardwareAccelerationEnabled() )
                        return decoder.release();
                }
                NX_LOG( lit("Hardware acceleration is not supported. Switching to software decoding..."), cl_logWARNING );
            }
        }

        case FFMPEG:
        default:
        {
            return new QnFfmpegVideoDecoder( data->compressionType, data, mtDecoding, &swDecoderCount );
        }
    }

    return nullptr;
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
