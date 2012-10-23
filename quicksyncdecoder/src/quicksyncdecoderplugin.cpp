////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "quicksyncdecoderplugin.h"

#include <DxErr.h>

#include <QApplication>
#include <QtPlugin>
#include <QMutexLocker>

#include <plugins/videodecoders/predefinedusagecalculator.h>
#include <plugins/videodecoders/videodecoderswitcher.h>

#include "quicksyncvideodecoder.h"

//!if defined, a mfx session is created in QuicksyncDecoderPlugin and all mfx sessions are joined with this one
//#define USE_PARENT_MFX_SESSION
//#define USE_SINGLE_DEVICE_PER_DECODER
#define USE_D3D9DEVICE_POOL
#ifdef USE_D3D9DEVICE_POOL
static const size_t d3d9devicePoolSize = 16;
#endif


static const char* QUICKSYNC_PLUGIN_ID = "45D92FCC-2B59-431e-BFF9-E11F2D6213DA";
//{ 0x45d92fcc, 0x2b59, 0x431e, { 0xbf, 0xf9, 0xe1, 0x1f, 0x2d, 0x62, 0x13, 0xda } };
static const char* DECODER_NAME = "quicksync";

//!name of xml file with quicksync decoder usage parameters
static QString quicksyncXmlFileName = "hw_decoding_conf.xml";

QuicksyncDecoderPlugin::QuicksyncDecoderPlugin()
:
    m_adapterNumber( 0 ),
    m_hardwareAccelerationEnabled( false ),
    m_direct3D9( NULL ),
    m_initialized( false ),
    m_prevD3DOperationResult( S_OK )
{
    OSVERSIONINFOEX osVersionInfo;
    memset( &osVersionInfo, 0, sizeof(osVersionInfo) );
    osVersionInfo.dwOSVersionInfoSize = sizeof(osVersionInfo);
    GetVersionEx( (LPOSVERSIONINFO)&osVersionInfo );
    m_osName = winVersionToName( osVersionInfo );

    readCPUInfo();
}

QuicksyncDecoderPlugin::~QuicksyncDecoderPlugin()
{
    closeD3D9Device();

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

bool QuicksyncDecoderPlugin::initialized() const
{
    if( !m_initialized )
    {
        //have to perform this terrible delayed initialization because logging does not work till initializeLog call
        m_initialized = initialize();
    }

    return m_initialized;
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
        m_initialized = initialize();
    }

    return m_hardwareAccelerationEnabled;
}

//!Implementation of QnAbstractVideoDecoderPlugin::create
QnAbstractVideoDecoder* QuicksyncDecoderPlugin::create(
    CodecID codecID,
    const QnCompressedVideoDataPtr& data,
    const QGLContext* const /*glContext*/,
    int currentSWDecoderCount ) const
{
    QMutexLocker lk( &m_mutex );

//#ifdef _DEBUG
//    return NULL;
//#endif

    if( !m_initialized )
    {
        //have to perform this terrible delayed initialization because logging does not work till initializeLog call
        m_initialized = initialize();
    }

    if( !m_hardwareAccelerationEnabled || (codecID != CODEC_ID_H264) )
        return NULL;

    Q_ASSERT( !m_d3dDevices.empty() );
    D3D9DeviceContext d3d9Ctx;
#ifdef USE_D3D9DEVICE_POOL
    if( m_d3dDevices.size() < d3d9devicePoolSize )
        openD3D9Device();
    //TODO/IMPL select device with min ref count
    d3d9Ctx = m_d3dDevices[rand() % m_d3dDevices.size()];
#elif defined(USE_SINGLE_DEVICE_PER_DECODER)
    if( !openD3D9Device() )
    {
        NX_LOG( QString::fromAscii("QuicksyncDecoderPlugin. Failed to create D3D device. %1").
            arg(QString::fromWCharArray(DXGetErrorDescription(m_prevD3DOperationResult))), cl_logERROR );
        return NULL;
    }
    d3d9Ctx = m_d3dDevices.back();
#else
    d3d9Ctx = m_d3dDevices[0];
#endif

    NX_LOG( QString::fromAscii("QuicksyncDecoderPlugin. Creating decoder..."), cl_logINFO );

    //parsing media sequence header to determine necessary parameters
    std::auto_ptr<QuickSyncVideoDecoder> decoder( new QuickSyncVideoDecoder(
        m_mfxSession.get(),
        d3d9Ctx.d3d9manager,
        m_usageWatcher.get(),
        m_adapterNumber ) );

    mfxVideoParam videoParam;
    memset( &videoParam, 0, sizeof(videoParam) );
    DecoderStreamDescription desc;
    if( decoder->readSequenceHeader( data, &videoParam ) )
    {
        desc.put( DecoderParameter::framePictureWidth, videoParam.mfx.FrameInfo.Width );
        desc.put( DecoderParameter::framePictureHeight, videoParam.mfx.FrameInfo.Height );
        desc.put( DecoderParameter::framePictureSize, videoParam.mfx.FrameInfo.Width*videoParam.mfx.FrameInfo.Height );
        desc.put( DecoderParameter::fps, videoParam.mfx.FrameInfo.FrameRateExtN / (float)videoParam.mfx.FrameInfo.FrameRateExtD );
        desc.put( DecoderParameter::pixelsPerSecond, 
            (videoParam.mfx.FrameInfo.FrameRateExtN / (float)videoParam.mfx.FrameInfo.FrameRateExtD) * videoParam.mfx.FrameInfo.Width*videoParam.mfx.FrameInfo.Height );
        desc.put( DecoderParameter::videoMemoryUsage, decoder->estimateSurfaceMemoryUsage() );
    }

    //filling stream parameters
    desc.put( DecoderParameter::sdkVersion, m_sdkVersionStr );
    desc.put( DecoderParameter::decoderName, DECODER_NAME );
    desc.put( DecoderParameter::osName, m_osName );
    desc.put( DecoderParameter::cpuString, m_cpuString );
    desc.put( DecoderParameter::totalCurrentNumberOfDecoders, currentSWDecoderCount+m_usageWatcher->currentSessionCount() );

    stree::MultiSourceResourceReader mediaStreamParams( &desc, m_graphicsDesc.get() );

    //retrieving existing sessions' info
    stree::ResourceContainer curUsageParams = m_usageWatcher->currentTotalUsage();
    //adding availableVideoMemory param
    quint64 newStreamEstimatedVideoMemoryUsage = 0;
    mediaStreamParams.getTypedVal( DecoderParameter::videoMemoryUsage, &newStreamEstimatedVideoMemoryUsage );
    UINT availableVideoMem = d3d9Ctx.d3d9Device->GetAvailableTextureMem();
    availableVideoMem = availableVideoMem > newStreamEstimatedVideoMemoryUsage
        ? (availableVideoMem - newStreamEstimatedVideoMemoryUsage)
        : 0;
    curUsageParams.put( DecoderParameter::availableVideoMemory, availableVideoMem );

#ifdef USE_SINGLE_DEVICE_PER_DECODER
    //releasing ref to d3d device, so it will be destroyed when decoder done using it
    m_d3dDevices.pop_back();
    ULONG refCount = d3d9Ctx.d3d9manager->Release();
    refCount = d3d9Ctx.d3d9Device->Release();
#endif

    //joining media stream parameters with GPU description
    if( !m_usageCalculator->isEnoughHWResourcesForAnotherDecoder( mediaStreamParams, curUsageParams ) )
        return NULL;

    decoder->decode( data, NULL );
    if( !decoder->isHardwareAccelerationEnabled() )
        return NULL;

    return new VideoDecoderSwitcher( decoder.release(), data );
}

bool QuicksyncDecoderPlugin::initialize() const
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
        NX_LOG( QString::fromAscii("Failed to create Intel media SDK parent session. Status %1").arg(status), cl_logERROR );
        m_mfxSession.reset();
        return false;
    }

    status = m_mfxSession->QueryVersion( &version );
    if( status != MFX_ERR_NONE )
    {
        NX_LOG( QString::fromAscii("Failed to read Intel media SDK version. Status %1").arg(status), cl_logERROR );
        m_mfxSession.reset();
        return false;
    }
    m_sdkVersionStr = QString("%1.%2").arg(version.Major).arg(version.Minor);

    mfxIMPL actualImplUsed = MFX_IMPL_HARDWARE;
    m_mfxSession->QueryIMPL( &actualImplUsed );
    if( actualImplUsed == MFX_IMPL_SOFTWARE )
    {
        m_hardwareAccelerationEnabled = false;
        return false;
    }

#ifndef USE_PARENT_MFX_SESSION
    m_mfxSession.reset();
#endif

    m_adapterNumber = (actualImplUsed >= MFX_IMPL_HARDWARE2 && actualImplUsed <= MFX_IMPL_HARDWARE4)
        ? (actualImplUsed - MFX_IMPL_HARDWARE2 + 1)
        : 0;

    if( !m_direct3D9 )
    {
        m_prevD3DOperationResult = Direct3DCreate9Ex( D3D_SDK_VERSION, &m_direct3D9 );
        if( m_prevD3DOperationResult != S_OK )
        {
            NX_LOG( QString::fromAscii("Failed to initialize IDirect3D9Ex. %1").arg(QString::fromWCharArray(DXGetErrorDescription(m_prevD3DOperationResult))), cl_logERROR );
            return false;
        }
    }

    //creating D3D device & D3D device manager
    if( !openD3D9Device() )
    {
        NX_LOG( QString::fromAscii("Failed to initialize IDirect3DDevice9Ex. %1").arg(QString::fromWCharArray(DXGetErrorDescription(m_prevD3DOperationResult))), cl_logERROR );
        return false;
    }

    m_hardwareAccelerationEnabled = true;

    m_graphicsDesc.reset( new D3DGraphicsAdapterDescription( m_direct3D9, m_adapterNumber ) );

    return true;
}

static BOOL CALLBACK enumWindowsProc( HWND hwnd, LPARAM lParam )
{
    std::list<HWND>* processTopLevelWindows = (std::list<HWND>*)lParam;

    DWORD processId = 0;
    GetWindowThreadProcessId( hwnd, &processId );
    if( processId == GetCurrentProcessId() )
        processTopLevelWindows->push_back( hwnd );
    return TRUE;
}

bool QuicksyncDecoderPlugin::openD3D9Device() const
{
    D3D9DeviceContext d3D9DeviceContext;

    //searching for a window handle
    std::list<HWND> processTopLevelWindows;
    HWND processWindow = NULL;
    EnumWindows( enumWindowsProc, (LPARAM)&processTopLevelWindows );
    if( processTopLevelWindows.empty() )
        return false;   //could not find window
    processWindow = processTopLevelWindows.front();
    HDC dc = GetDC( processWindow );

    //creating device
    D3DPRESENT_PARAMETERS presentationParameters;
    memset( &presentationParameters, 0, sizeof(presentationParameters) );
    presentationParameters.BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
    presentationParameters.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
    presentationParameters.BackBufferFormat = D3DFMT_X8R8G8B8;  //(D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' );
    presentationParameters.BackBufferCount = 1;
    //presentationParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
    //presentationParameters.MultiSampleQuality = 0;
    presentationParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;  //D3DSWAPEFFECT_COPY;
    presentationParameters.hDeviceWindow = processWindow;
    presentationParameters.Windowed = TRUE;
    presentationParameters.EnableAutoDepthStencil = FALSE;
    presentationParameters.Flags = D3DPRESENTFLAG_VIDEO;
    presentationParameters.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT; //since WINDOWED
    presentationParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    m_prevD3DOperationResult = m_direct3D9->CreateDeviceEx(
        m_adapterNumber,
        D3DDEVTYPE_HAL,
        processWindow,
        D3DCREATE_HARDWARE_VERTEXPROCESSING /*D3DCREATE_SOFTWARE_VERTEXPROCESSING*/ | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &presentationParameters,
        NULL,
        &d3D9DeviceContext.d3d9Device );
    if( m_prevD3DOperationResult != S_OK )
    {
        ReleaseDC( processWindow, dc );
        return false;
    }

    m_prevD3DOperationResult = d3D9DeviceContext.d3d9Device->ResetEx( &presentationParameters, NULL );
    if( m_prevD3DOperationResult != S_OK )
    {
        ReleaseDC( processWindow, dc );
        d3D9DeviceContext.d3d9Device->Release();
        return false;
    }

    //creating device manager
    m_prevD3DOperationResult = DXVA2CreateDirect3DDeviceManager9( &d3D9DeviceContext.deviceResetToken, &d3D9DeviceContext.d3d9manager );
    if( m_prevD3DOperationResult != S_OK )
    {
        ReleaseDC( processWindow, dc );
        d3D9DeviceContext.d3d9Device->Release();
        return false;
    }

    //resetting manager with device
    m_prevD3DOperationResult = d3D9DeviceContext.d3d9manager->ResetDevice( d3D9DeviceContext.d3d9Device, d3D9DeviceContext.deviceResetToken );
    if( m_prevD3DOperationResult != S_OK )
    {
        ReleaseDC( processWindow, dc );
        d3D9DeviceContext.d3d9manager->Release();
        d3D9DeviceContext.d3d9Device->Release();
        return false;
    }

    //ULONG refCount = d3D9DeviceContext.d3d9Device->Release();

    ReleaseDC( processWindow, dc );

    m_d3dDevices.push_back( d3D9DeviceContext );
    return true;
}

void QuicksyncDecoderPlugin::closeD3D9Device()
{
    for( std::vector<D3D9DeviceContext>::size_type
        i = 0;
        i < m_d3dDevices.size();
        ++i )
    {
        //releasing device manager
        if( m_d3dDevices[i].d3d9manager )
        {
            ULONG refCount = m_d3dDevices[i].d3d9manager->Release();
            if( refCount > 0 )
                int x = 0;
        }

        //closing device
        if( m_d3dDevices[i].d3d9Device )
        {
            ULONG refCount = m_d3dDevices[i].d3d9Device->Release();
            if( refCount > 0 )
                int x = 0;
        }
    }

    m_d3dDevices.clear();
}

QString QuicksyncDecoderPlugin::winVersionToName( const OSVERSIONINFOEX& osVersionInfo ) const
{
//Windows 8	                6.2	    6	2	OSVERSIONINFOEX.wProductType == VER_NT_WORKSTATION
//Windows Server 2012	    6.2	    6	2	OSVERSIONINFOEX.wProductType != VER_NT_WORKSTATION
//Windows 7	                6.1	    6	1	OSVERSIONINFOEX.wProductType == VER_NT_WORKSTATION
//Windows Server 2008 R2	6.1	    6	1	OSVERSIONINFOEX.wProductType != VER_NT_WORKSTATION
//Windows Server 2008	    6.0 	6	0	OSVERSIONINFOEX.wProductType != VER_NT_WORKSTATION
//Windows Vista	            6.0 	6	0	OSVERSIONINFOEX.wProductType == VER_NT_WORKSTATION
//Windows Server 2003 R2	5.2	    5	2	GetSystemMetrics(SM_SERVERR2) != 0
//Windows Server 2003	    5.2 	5	2	GetSystemMetrics(SM_SERVERR2) == 0
//Windows XP	            5.1	    5	1	Not applicable
//Windows 2000	            5.0	    5	0	Not applicable

    QString osName;
    switch( osVersionInfo.dwMajorVersion )
    {
        case 6:
            switch( osVersionInfo.dwMinorVersion )
            {
                case 2:
                    osName = osVersionInfo.wProductType == VER_NT_WORKSTATION ? "Windows 8" : "Windows Server 2012";
                    break;
                case 1:
                    osName = osVersionInfo.wProductType == VER_NT_WORKSTATION ? "Windows 7" : "Windows Server 2008 R2";
                    break;
                case 0:
                    osName = osVersionInfo.wProductType == VER_NT_WORKSTATION ? "Windows Vista" : "Windows Server 2008";
                    break;
                default:
                    return QString("%1.%2 %3").arg(osVersionInfo.dwMajorVersion).arg(osVersionInfo.dwMinorVersion).arg(QString::fromWCharArray(osVersionInfo.szCSDVersion));
            }
            break;

        case 5:
            switch( osVersionInfo.dwMinorVersion )
            {
                case 2:
                    osName = GetSystemMetrics(SM_SERVERR2) != 0 ? "Windows Server 2003 R2" : "Windows Server 2003";
                    break;
                case 1:
                    osName = "Windows XP";
                    break;
                case 0:
                    osName = "Windows 2000";
                    break;
                default:
                    return QString("%1.%2 %3").arg(osVersionInfo.dwMajorVersion).arg(osVersionInfo.dwMinorVersion).arg(QString::fromWCharArray(osVersionInfo.szCSDVersion));
            }
            break;

        default:
            return QString("%1.%2 %3").arg(osVersionInfo.dwMajorVersion).arg(osVersionInfo.dwMinorVersion).arg(QString::fromWCharArray(osVersionInfo.szCSDVersion));
    }

    return QString("%1 %2 (%3.%4)").arg(osName).arg(QString::fromWCharArray(osVersionInfo.szCSDVersion)).
        arg(osVersionInfo.dwMajorVersion).arg(osVersionInfo.dwMinorVersion);
}

void QuicksyncDecoderPlugin::readCPUInfo()
{
    char CPUBrandString[0x40];
    int CPUInfo[4] = {-1};

    // Calling __cpuid with 0x80000000 as the InfoType argument
    // gets the number of valid extended IDs.
    __cpuid(CPUInfo, 0x80000000);
    unsigned int nExIds = CPUInfo[0];
    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    // Get the information associated with each extended ID.
    for( unsigned int i=0x80000000; i<=nExIds; ++i )
    {
        __cpuid(CPUInfo, i);

        // Interpret CPU brand string and cache information.
        if  (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }

    if( nExIds >= 0x80000004 )
    {
        //printf_s("\nCPU Brand String: %s\n", CPUBrandString);
        m_cpuString = QString::fromAscii( CPUBrandString ).trimmed();
    }
}

Q_EXPORT_PLUGIN2( quicksyncdecoderplugin, QuicksyncDecoderPlugin );
