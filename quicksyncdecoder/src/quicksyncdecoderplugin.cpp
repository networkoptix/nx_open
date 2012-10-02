////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "quicksyncdecoderplugin.h"

#include <DxErr.h>

#include <QApplication>
#include <QtPlugin>

#include <plugins/videodecoders/predefinedusagecalculator.h>
#include <plugins/videodecoders/videodecoderswitcher.h>

#include "quicksyncvideodecoder.h"


static const char* QUICKSYNC_PLUGIN_ID = "45D92FCC-2B59-431e-BFF9-E11F2D6213DA";
//{ 0x45d92fcc, 0x2b59, 0x431e, { 0xbf, 0xf9, 0xe1, 0x1f, 0x2d, 0x62, 0x13, 0xda } };

//!name of xml file with quicksync decoder usage parameters
static QString quicksyncXmlFileName = "quicksync.xml";

QuicksyncDecoderPlugin::QuicksyncDecoderPlugin()
:
    m_adapterNumber( 0 ),
    m_hardwareAccelerationEnabled( false ),
    m_direct3D9( NULL ),
    m_initialized( false )
{
}

QuicksyncDecoderPlugin::~QuicksyncDecoderPlugin()
{
    if( m_direct3D9 )
    {
        m_direct3D9->Release();
        m_direct3D9 = NULL;
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
    if( !m_initialized )
    {
        //have to perform this terrible delayed initialization because logging does not work till initializeLog call
        initialize();
        m_initialized = true;
    }

    return m_hardwareAccelerationEnabled;
}

//!Implementation of QnAbstractVideoDecoderPlugin::create
QnAbstractVideoDecoder* QuicksyncDecoderPlugin::create(
    CodecID codecID,
    const QnCompressedVideoDataPtr& data,
    const QGLContext* const /*glContext*/ ) const
{
    if( !m_initialized )
    {
        //have to perform this terrible delayed initialization because logging does not work till initializeLog call
        initialize();
        m_initialized = true;
    }

    if( !m_hardwareAccelerationEnabled || (codecID != CODEC_ID_H264) )
        return NULL;

    cl_log.log( QString::fromAscii("QuicksyncDecoderPlugin. Creating decoder..."), cl_logINFO );

    //TODO/IMPL parse media sequence header to determine necessary parameters

    //filling stream parameters
    DecoderStreamDescription desc;
    desc.put( DecoderParameter::framePictureWidth, std::max<>( data->width, 0 ) );
    desc.put( DecoderParameter::framePictureHeight, std::max<>( data->height, 0 ) );
    desc.put( DecoderParameter::framePictureSize, std::max<>( data->width*data->height, 0 ) );
    desc.put( DecoderParameter::fps, 30 );
    desc.put( DecoderParameter::pixelsPerSecond, 30 * std::max<>( data->width*data->height, 0 ) );
    desc.put( DecoderParameter::videoMemoryUsage, 0 );
    desc.put( DecoderParameter::sdkVersion, m_sdkVersionStr );

    //joining media stream parameters with GPU description
    if( !m_usageCalculator->isEnoughHWResourcesForAnotherDecoder( stree::MultiSourceResourceReader( &desc, m_graphicsDesc.get() ) ) )
        return NULL;

    std::auto_ptr<QuickSyncVideoDecoder> decoder( new QuickSyncVideoDecoder(
        m_mfxSession.get(),
        m_direct3D9,
        data,
        m_usageWatcher.get(),
        m_adapterNumber ) );
    if( !decoder->isHardwareAccelerationEnabled() )
        return NULL;

    return new VideoDecoderSwitcher( decoder.release(), data );
}

void QuicksyncDecoderPlugin::initialize() const
{
    m_usageWatcher.reset( new PluginUsageWatcher( QByteArray( QUICKSYNC_PLUGIN_ID ) ) );
    m_usageCalculator.reset( new PredefinedUsageCalculator(
        m_resNameset,
        QString("%1/%2").arg(QApplication::instance()->applicationDirPath()).arg(quicksyncXmlFileName),
        m_usageWatcher.get() ) );

    //opening parent mfx session
    m_mfxSession.reset( new MFXVideoSession() );
    mfxVersion version;
    memset( &version, 0, sizeof(version) );
    version.Major = 1;
    mfxStatus status = m_mfxSession->Init( MFX_IMPL_AUTO_ANY, &version );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to create Intel media SDK parent session. Status %1").arg(status), cl_logERROR );
        m_mfxSession.reset();
        return;
    }

    status = m_mfxSession->QueryVersion( &version );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to read Intel media SDK version. Status %1").arg(status), cl_logERROR );
        m_mfxSession.reset();
        return;
    }
    m_sdkVersionStr = QString("%1.%2").arg(version.Major).arg(version.Minor);

    mfxIMPL actualImplUsed = MFX_IMPL_HARDWARE;
    m_mfxSession->QueryIMPL( &actualImplUsed );
    if( actualImplUsed == MFX_IMPL_SOFTWARE )
    {
        m_hardwareAccelerationEnabled = false;
        return;
    }
    m_adapterNumber = (actualImplUsed >= MFX_IMPL_HARDWARE2 && actualImplUsed <= MFX_IMPL_HARDWARE4)
        ? (actualImplUsed - MFX_IMPL_HARDWARE2 + 1)
        : 0;

    HRESULT d3d9Result = Direct3DCreate9Ex( D3D_SDK_VERSION, &m_direct3D9 );
    if( d3d9Result != S_OK )
    {
        cl_log.log( QString::fromAscii("Failed to initialize IDirect3D9Ex. %1").arg(QString::fromWCharArray(DXGetErrorDescription(d3d9Result))), cl_logERROR );
        return;
    }

    m_hardwareAccelerationEnabled = true;

    m_graphicsDesc.reset( new D3DGraphicsAdapterDescription( m_direct3D9, m_adapterNumber ) );
}

Q_EXPORT_PLUGIN2( quicksyncdecoderplugin, QuicksyncDecoderPlugin );
