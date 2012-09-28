////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "quicksyncdecoderplugin.h"

#include <QtPlugin>

#include <plugins/videodecoders/predefinedusagecalculator.h>
#include <plugins/videodecoders/videodecoderswitcher.h>

#include "quicksyncvideodecoder.h"


static const char* QUICKSYNC_PLUGIN_ID = "45D92FCC-2B59-431e-BFF9-E11F2D6213DA";
//{ 0x45d92fcc, 0x2b59, 0x431e, { 0xbf, 0xf9, 0xe1, 0x1f, 0x2d, 0x62, 0x13, 0xda } };

QuicksyncDecoderPlugin::QuicksyncDecoderPlugin()
{
    m_usageWatcher.reset( new PluginUsageWatcher( QByteArray( QUICKSYNC_PLUGIN_ID ) ) );
    m_usageCalculator.reset( new PredefinedUsageCalculator( QString(), m_usageWatcher.get() ) );

    //TODO/IMPL get GPU description

    //TODO/IMPL opening parent mfx session
    //opening media session
    m_mfxSession.reset( new MFXVideoSession() );
    mfxVersion version;
    memset( &version, 0, sizeof(version) );
    version.Major = 1;
    mfxStatus status = m_mfxSession->Init( MFX_IMPL_AUTO_ANY, &version );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to create Intel media SDK parent session. Status %1").arg(status), cl_logERROR );
        m_mfxSession.reset();
    }
}

quint32 QuicksyncDecoderPlugin::minSupportedVersion() const
{
    //TODO/IMPL
    return 1<<24;
}

void QuicksyncDecoderPlugin::initializeLog( QnLog* logInstance )
{
    QnLog::initLog( logInstance );
}

//!Implementation of QnAbstractVideoDecoderPlugin::supportedCodecTypes
QList<CodecID> QuicksyncDecoderPlugin::supportedCodecTypes() const
{
    QList<CodecID> codecList;
    codecList.push_back( CODEC_ID_H264 );
    return codecList;
}

//!Implementation of QnAbstractVideoDecoderPlugin::isHardwareAccelerated
bool QuicksyncDecoderPlugin::isHardwareAccelerated() const
{
    if( !m_mfxSession.get() )
        return false;

    mfxIMPL actualImplUsed = MFX_IMPL_HARDWARE;
    m_mfxSession->QueryIMPL( &actualImplUsed );
    return actualImplUsed != MFX_IMPL_SOFTWARE;
}

//!Implementation of QnAbstractVideoDecoderPlugin::create
QnAbstractVideoDecoder* QuicksyncDecoderPlugin::create(
    CodecID codecID,
    const QnCompressedVideoDataPtr& data,
    const QGLContext* const /*glContext*/ ) const
{
    if( codecID != CODEC_ID_H264 )
        return NULL;

    cl_log.log( QString::fromAscii("QuicksyncDecoderPlugin. Creating decoder..."), cl_logINFO );

    //TODO/IMPL parse media sequence header to determine necessary parameters

    //TODO/IMPL filling stream parameters
    DecoderStreamDescription desc;
    desc.put( DecoderParameter::framePictureSize, data->width*data->height );

    //TODO/IMPL join media stream parameters with GPU description

    if( !m_usageCalculator->isEnoughHWResourcesForAnotherDecoder( desc ) )
        return NULL;

    std::auto_ptr<QuickSyncVideoDecoder> decoder( new QuickSyncVideoDecoder(
        m_mfxSession.get(),
        data,
        m_usageWatcher.get() ) );
    if( !decoder->isHardwareAccelerationEnabled() )
        return NULL;

    return new VideoDecoderSwitcher( decoder.release(), data );
}

Q_EXPORT_PLUGIN2( quicksyncdecoderplugin, QuicksyncDecoderPlugin );
