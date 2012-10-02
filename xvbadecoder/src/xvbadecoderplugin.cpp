////////////////////////////////////////////////////////////
// 22 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#include "xvbadecoderplugin.h"

#include <QtPlugin>

#include <plugins/videodecoders/predefinedusagecalculator.h>
#include <plugins/videodecoders/videodecoderswitcher.h>

#include "xvbadecoder.h"


static const char* XVBADECODER_PLUGIN_ID = "45D92FCC-2B59-431e-BFF9-E11F2D6213DA";
//{ 0x45d92fcc, 0x2b59, 0x431e, { 0xbf, 0xf9, 0xe1, 0x1f, 0x2d, 0x62, 0x13, 0xda } };

//!name of xml file with quicksync decoder usage parameters
static QString hwDecoderXmlFileName = "quicksync.xml";

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

QList<CodecID> QnXVBADecoderPlugin::supportedCodecTypes() const
{
    QList<CodecID> codecList;
    codecList.push_back( CODEC_ID_H264 );
    return codecList;
}

bool QnXVBADecoderPlugin::isHardwareAccelerated() const
{
    return m_hardwareAccelerationEnabled;
}

QnAbstractVideoDecoder* QnXVBADecoderPlugin::create(
    CodecID codecID,
    const QnCompressedVideoDataPtr& data,
    const QGLContext* const glContext ) const
{
    if( codecID != CODEC_ID_H264 )
        return NULL;

    cl_log.log( QString::fromAscii("XVBADecoderPlugin. Creating decoder..."), cl_logINFO );

    //parsing media sequence header to determine necessary parameters
    std::auto_ptr<QnXVBADecoder> decoder( new QnXVBADecoder( glContext, data, m_usageWatcher.get() ) );
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

    //joining media stream parameters with GPU description
    if( !m_usageCalculator->isEnoughHWResourcesForAnotherDecoder( stree::MultiSourceResourceReader( &desc, m_graphicsDesc.get() ) ) )
        return NULL;

    return new VideoDecoderSwitcher( decoder.release(), data );
}

Q_EXPORT_PLUGIN2( xvbadecoderplugin, QnXVBADecoderPlugin );
