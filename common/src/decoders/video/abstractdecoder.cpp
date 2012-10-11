
#include "abstractdecoder.h"

#include <iostream>
#include <memory>

#include <libavcodec/avcodec.h>

#include "ffmpeg.h"
#include "ipp_h264_decoder.h"
//#include "quicksyncvideodecoder.h"
//#include "xvbadecoder.h"
#include "../abstractvideodecoderplugin.h"
#include "../../plugins/pluginmanager.h"


CLVideoDecoderFactory::CLCodecManufacture CLVideoDecoderFactory::m_codecManufacture = AUTO;

QnAbstractVideoDecoder* CLVideoDecoderFactory::createDecoder( const QnCompressedVideoDataPtr data, bool mtDecoding, const QGLContext* glContext )
{
    // h264 
    switch( m_codecManufacture )
    {
	#ifdef Q_OS_WIN
		case INTELIPP:
			return new IPPH264Decoder();
	#endif

		case AUTO:
		{
            //searching for a video decoder with hardware acceleration, supporting codec type data->compressionType
            QnAbstractVideoDecoderPlugin* videoDecoderPlugin = NULL;
            const QList<QnAbstractVideoDecoderPlugin*>& plugins = PluginManager::instance()->findPlugins<QnAbstractVideoDecoderPlugin>();
            foreach( QnAbstractVideoDecoderPlugin* plugin, plugins )
            {
                if( !plugin->isHardwareAccelerated() || plugin->supportedCodecTypes().indexOf(data->compressionType) == -1 )
                    continue;
                videoDecoderPlugin = plugin;
                break;
            }
            if( videoDecoderPlugin )
            {
                std::auto_ptr<QnAbstractVideoDecoder> decoder( videoDecoderPlugin->create( data->compressionType, data, glContext ) );
                if( decoder.get() && decoder->isHardwareAccelerationEnabled() )
                    return decoder.release();
            }
            cl_log.log( QString::fromAscii("Hardware acceleration is not supported. Switching to software decoding..."), cl_logWARNING );
		}

        case FFMPEG:
        default:
            return new CLFFmpegVideoDecoder( data->compressionType, data, mtDecoding );
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
