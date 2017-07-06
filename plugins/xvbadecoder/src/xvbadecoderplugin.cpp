////////////////////////////////////////////////////////////
// 22 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#include "xvbadecoderplugin.h"

#include <QtPlugin>

#include <plugins/videodecoders/predefinedusagecalculator.h>
#include <plugins/videodecoders/videodecoderswitcher.h>

#include "xvbadecoder.h"


static const char* XVBADECODER_PLUGIN_ID = "C9037D6C-4FFC-4dd8-8849-4A3A44C93091";
//{ 0xc9037d6c, 0x4ffc, 0x4dd8, { 0x88, 0x49, 0x4a, 0x3a, 0x44, 0xc9, 0x30, 0x91 } };
static const char* DECODER_NAME = "xvba";


//!name of xml file with quicksync decoder usage parameters
static QString hwDecoderXmlFileName = "hw_decoding_conf.xml";

QnXVBADecoderPlugin::QnXVBADecoderPlugin()
:
    m_hardwareAccelerationEnabled( false )
{
    int version = 0;
    m_hardwareAccelerationEnabled = XVBAQueryExtension( QX11Info::display(), &version );
    if( !m_hardwareAccelerationEnabled )
    {
        cl_log.log( QString::fromAscii("XVBA decode acceleration is not supported on host system"), cl_logERROR );
        return;
    }
    cl_log.log( QString::fromAscii("XVBA of version %1.%2 found").arg(version >> 16).arg(version & 0xffff), cl_logINFO );
    m_sdkVersionStr = QString::fromAscii("%1.%2").arg(version >> 16).arg(version & 0xffff);

    m_usageWatcher.reset( new PluginUsageWatcher( QByteArray( XVBADECODER_PLUGIN_ID ) ) );
    m_usageCalculator.reset( new PredefinedUsageCalculator(
        m_resNameset,
        QString("%1/%2").arg(QApplication::instance()->applicationDirPath()).arg(hwDecoderXmlFileName),
        m_usageWatcher.get() ) );
    m_graphicsDesc.reset( new LinuxGraphicsAdapterDescription( 0 ) );
}

quint32 QnXVBADecoderPlugin::minSupportedVersion() const
{
    //TODO/IMPL
    return 1<<24;   //1.0.0
}

void QnXVBADecoderPlugin::initializeLog( QnLog* externalLog )
{
    QnLog::initLog( externalLog );
}

QList<AVCodecID> QnXVBADecoderPlugin::supportedCodecTypes() const
{
    QList<AVCodecID> codecList;
    codecList.push_back( AV_CODEC_ID_H264 );
    return codecList;
}

bool QnXVBADecoderPlugin::isHardwareAccelerated() const
{
    return m_hardwareAccelerationEnabled;
}

QnAbstractVideoDecoder* QnXVBADecoderPlugin::create(
    AVCodecID codecID,
    const QnCompressedVideoDataPtr& data,
    const QGLContext* const glContext ) const
{
    if( codecID != AV_CODEC_ID_H264 )
        return NULL;

    cl_log.log( QString::fromAscii("XVBADecoderPlugin. Creating decoder..."), cl_logINFO );

    //parsing media sequence header to determine necessary parameters
    std::unique_ptr<QnXVBADecoder> decoder( new QnXVBADecoder( glContext, data, m_usageWatcher.get() ) );
    if( !decoder->isHardwareAccelerationEnabled() )
        return NULL;

    //filling stream parameters
    DecoderStreamDescription desc;
    desc.put( DecoderParameter::framePictureWidth, std::max<>( decoder->getWidth(), 0 ) );
    desc.put( DecoderParameter::framePictureHeight, std::max<>( decoder->getHeight(), 0 ) );
    desc.put( DecoderParameter::framePictureSize, std::max<>( decoder->getWidth()*decoder->getHeight(), 0 ) );
    desc.put( DecoderParameter::fps, 30 );  //TODO
    desc.put( DecoderParameter::pixelsPerSecond, 30 * std::max<>( data->width*data->height, 0 ) );
    desc.put( DecoderParameter::videoMemoryUsage, 0 );
    desc.put( DecoderParameter::sdkVersion, m_sdkVersionStr );
    desc.put( DecoderParameter::decoderName, DECODER_NAME );

    //joining media stream parameters with GPU description
    if( !m_usageCalculator->isEnoughHWResourcesForAnotherDecoder( nx::utils::stree::MultiSourceResourceReader( &desc, m_graphicsDesc.get() ) ) )
        return NULL;

    return new VideoDecoderSwitcher( decoder.release(), data );
}

Q_EXPORT_PLUGIN2( xvbadecoderplugin, QnXVBADecoderPlugin );
