/**********************************************************
* 14 sep 2012
* a.kolesnikov
***********************************************************/

#include "mfxallocator.h"

#include <dxerr.h>


//////////////////////////////////////////////////////////
// MFXBufferAllocator (example from Intel Media SDK )
//////////////////////////////////////////////////////////

typedef struct {
    //!these values are always aligned to 32-byte boundary top
    mfxU16 width, height;
    mfxU8* base;
    mfxU16 chromaFormat;
    mfxU32 fourCC;
} mid_struct;


//////////////////////////////////////////////////////////
// AbstractMFXBufferAllocator
//////////////////////////////////////////////////////////
AbstractMFXBufferAllocator::AbstractMFXBufferAllocator()
{
    memset( reserved, 0, sizeof(reserved) );
    pthis = this;
    Alloc = ba_alloc;
    Lock = ba_lock;
    Unlock = ba_unlock;
    Free = ba_free;
}

AbstractMFXBufferAllocator::~AbstractMFXBufferAllocator()
{
}

mfxStatus AbstractMFXBufferAllocator::ba_alloc( mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId* mid )
{
    return static_cast<AbstractMFXBufferAllocator*>(pthis)->alloc( nbytes, type, mid );
}

mfxStatus AbstractMFXBufferAllocator::ba_lock( mfxHDL pthis, mfxMemId mid, mfxU8 **ptr )
{
    return static_cast<AbstractMFXBufferAllocator*>(pthis)->lock( mid, ptr );
}

mfxStatus AbstractMFXBufferAllocator::ba_unlock( mfxHDL pthis, mfxMemId mid )
{
    return static_cast<AbstractMFXBufferAllocator*>(pthis)->unlock( mid );
}

mfxStatus AbstractMFXBufferAllocator::ba_free( mfxHDL pthis, mfxMemId mid )
{
    return static_cast<AbstractMFXBufferAllocator*>(pthis)->_free( mid );
}


//////////////////////////////////////////////////////////
// MFXBufferAllocator (example from Intel Media SDK )
//////////////////////////////////////////////////////////
mfxStatus MFXBufferAllocator::alloc( mfxU32 nbytes, mfxU16 /*type*/, mfxMemId* mid )
{
    *mid = malloc(nbytes);
    return (*mid) ? MFX_ERR_NONE : MFX_ERR_MEMORY_ALLOC;
}

mfxStatus MFXBufferAllocator::lock( mfxMemId mid, mfxU8 **ptr )
{
    *ptr = (mfxU8*)mid;
    return MFX_ERR_NONE;
}

mfxStatus MFXBufferAllocator::unlock( mfxMemId /*mid*/ )
{
    return MFX_ERR_NONE;
}

mfxStatus MFXBufferAllocator::_free( mfxMemId mid ) 
{
    if (mid)
        free((mfxU8*)mid);
    return MFX_ERR_NONE;
}


//////////////////////////////////////////////////////////
// AbstractMFXFrameAllocator (example from Intel Media SDK )
//////////////////////////////////////////////////////////
AbstractMFXFrameAllocator::AbstractMFXFrameAllocator()
{
    memset( reserved, 0, sizeof(reserved) );
    pthis = this;

    Alloc = fa_alloc;
    Lock = fa_lock;
    Unlock = fa_unlock;
    GetHDL = fa_gethdl;
    Free = fa_free;
}

AbstractMFXFrameAllocator::~AbstractMFXFrameAllocator()
{
}

mfxStatus AbstractMFXFrameAllocator::fa_alloc( mfxHDL pthis, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response )
{
    return static_cast<AbstractMFXFrameAllocator*>(pthis)->alloc( request, response );
}

mfxStatus AbstractMFXFrameAllocator::fa_lock( mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr )
{
    return static_cast<AbstractMFXFrameAllocator*>(pthis)->lock( mid, ptr );
}

mfxStatus AbstractMFXFrameAllocator::fa_unlock( mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr )
{
    return static_cast<AbstractMFXFrameAllocator*>(pthis)->unlock( mid, ptr );
}

mfxStatus AbstractMFXFrameAllocator::fa_gethdl( mfxHDL pthis, mfxMemId mid, mfxHDL* handle )
{
    return static_cast<AbstractMFXFrameAllocator*>(pthis)->gethdl( mid, handle );
}

mfxStatus AbstractMFXFrameAllocator::fa_free( mfxHDL pthis, mfxFrameAllocResponse* response )
{
    return static_cast<AbstractMFXFrameAllocator*>(pthis)->_free( response );
}


//////////////////////////////////////////////////////////
// MFXFrameAllocator (example from Intel Media SDK )
//////////////////////////////////////////////////////////
mfxStatus MFXFrameAllocator::alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response )
{
    if( !(request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) )
        return MFX_ERR_UNSUPPORTED;
    if( (request->Info.FourCC != MFX_FOURCC_NV12) && (request->Info.FourCC != MFX_FOURCC_YV12) )
        return MFX_ERR_UNSUPPORTED;
    response->NumFrameActual = request->NumFrameMin;
    response->mids = (mfxMemId*)malloc( response->NumFrameActual * sizeof(mid_struct*) );
    for( int i=0; i<request->NumFrameMin; ++i )
    {
        mid_struct* mmid = (mid_struct *)malloc(sizeof(mid_struct));
        mmid->width = ALIGN16( request->Info.Width );
        mmid->height = ALIGN32( request->Info.Height );
        mmid->chromaFormat = request->Info.ChromaFormat;
        mmid->fourCC = request->Info.FourCC;
        if( request->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV420 )
            mmid->base = (mfxU8*)malloc(mmid->width*mmid->height*3/2);
        else if( request->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV422 )
            mmid->base = (mfxU8*)malloc(mmid->width*mmid->height*2);
        response->mids[i] = mmid;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXFrameAllocator::lock( mfxMemId mid, mfxFrameData* ptr )
{
    //works only for YUV
    mid_struct* mmid = (mid_struct *)mid;
    ptr->Pitch = mmid->width;
    ptr->Y = mmid->base;
    if( mmid->chromaFormat == MFX_CHROMAFORMAT_YUV420 )
    {
        ptr->U = ptr->Y + mmid->width * mmid->height;
        if( mmid->fourCC == MFX_FOURCC_NV12 )
            ptr->V = ptr->U + 1;
        else if( mmid->fourCC == MFX_FOURCC_YV12 )
            ptr->V = ptr->U + (mmid->width * mmid->height) / 4;
    }
    else if( mmid->chromaFormat == MFX_CHROMAFORMAT_YUV422 )
    {
        ptr->U = ptr->Y + mmid->width * mmid->height;
        ptr->V = ptr->U + (mmid->width * mmid->height) / 2;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXFrameAllocator::unlock( mfxMemId /*mid*/, mfxFrameData* ptr )
{
    if( ptr )
        ptr->Y = ptr->U = ptr->V = ptr->A = NULL;
    return MFX_ERR_NONE;
}

mfxStatus MFXFrameAllocator::gethdl( mfxMemId /*mid*/, mfxHDL* /*handle*/ )
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXFrameAllocator::_free( mfxFrameAllocResponse* response )
{
    for( int i = 0; i<response->NumFrameActual; i++ )
    {
        mid_struct* mmid = (mid_struct*)response->mids[i];
        free( mmid->base );
        free( mmid );
    }
    free( response->mids );
    return MFX_ERR_NONE;
}


//////////////////////////////////////////////////////////
// MFXDirect3DSurfaceAllocator
//////////////////////////////////////////////////////////
MFXDirect3DSurfaceAllocator::Direct3DSurfaceContext::Direct3DSurfaceContext()
:
    surface( NULL ),
    width( 0 ),
    height( 0 )
{
}

MFXDirect3DSurfaceAllocator::MFXDirect3DSurfaceAllocator()
:
    m_initialized( false ),
    m_direct3D9( NULL ),
    m_d3d9manager( NULL ),
    m_d3d9Device( NULL ),
    m_d3DeviceHandle( INVALID_HANDLE_VALUE ),
    m_decoderService( NULL ),
    m_deviceResetToken( 0 ),
    m_prevOperationResult( S_OK )
{
}

MFXDirect3DSurfaceAllocator::~MFXDirect3DSurfaceAllocator()
{
    if( !m_initialized )
        return;

    m_decoderService->Release();
    m_decoderService = NULL;

    closeD3D9Device();

    m_direct3D9->Release();
    m_direct3D9 = NULL;
}

mfxStatus MFXDirect3DSurfaceAllocator::alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response )
{
    DWORD renderTarget = 0;
    if( request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET )
        renderTarget = DXVA2_VideoDecoderRenderTarget;
    else if( request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET )
        renderTarget = DXVA2_VideoProcessorRenderTarget;
    else
        return MFX_ERR_UNSUPPORTED;

    IDirect3DSurface9** surfaceArray = new IDirect3DSurface9*[request->NumFrameSuggested+1];

    for( int i = 0; i < 2; ++i )
    {
        m_prevOperationResult = m_decoderService->CreateSurface(
            request->Info.Width,
            request->Info.Height,
            request->NumFrameSuggested,
            (D3DFORMAT)MAKEFOURCC('N','V','1','2'),
            D3DPOOL_DEFAULT,
            0,
            renderTarget,
            surfaceArray,
            NULL );
        switch( m_prevOperationResult )
        {
            case S_OK:
                break;

            case E_FAIL:
                if( resetDevice() )
                    continue;

            default:
                delete[] surfaceArray;
                return MFX_ERR_MEMORY_ALLOC;
        }
        break;
    }

    if( m_prevOperationResult != S_OK )
    {
        delete[] surfaceArray;
        return MFX_ERR_MEMORY_ALLOC;
    }

    Direct3DSurfaceContext** mids = new Direct3DSurfaceContext*[request->NumFrameSuggested+1];
    for( int i = 0; i < request->NumFrameSuggested+1; ++i )
    {
        mids[i] = new Direct3DSurfaceContext();
        mids[i]->surface = surfaceArray[i];
        mids[i]->width = request->Info.Width;
        mids[i]->height = request->Info.Height;
    }
    response->mids = reinterpret_cast<mfxMemId*>(mids);
    response->NumFrameActual = request->NumFrameSuggested+1;
    delete[] surfaceArray;

    return MFX_ERR_NONE;
}

mfxStatus MFXDirect3DSurfaceAllocator::lock( mfxMemId mid, mfxFrameData* ptr )
{
    D3DLOCKED_RECT lockedRect;
    memset( &lockedRect, 0, sizeof(lockedRect) );
    Direct3DSurfaceContext* surfaceCtx = static_cast<Direct3DSurfaceContext*>(mid);
    if( surfaceCtx->surface->LockRect( &lockedRect, NULL, 0 ) != D3D_OK )
        return MFX_ERR_LOCK_MEMORY;

    //assuming surface format is always NV12
    ptr->Pitch = (mfxU16)lockedRect.Pitch;
    ptr->Y = (mfxU8*)lockedRect.pBits;
    ptr->U = (mfxU8*)lockedRect.pBits + surfaceCtx->height * lockedRect.Pitch;
    ptr->V = ptr->U + 1;

    return MFX_ERR_NONE;
}

mfxStatus MFXDirect3DSurfaceAllocator::unlock( mfxMemId mid, mfxFrameData* ptr )
{
    Direct3DSurfaceContext* surfaceCtx = static_cast<Direct3DSurfaceContext*>(mid);
    surfaceCtx->surface->UnlockRect();
    if( ptr )
    {
        ptr->Pitch = 0;
        ptr->Y = NULL;
        ptr->U = NULL;
        ptr->V = NULL;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXDirect3DSurfaceAllocator::gethdl( mfxMemId mid, mfxHDL* handle )
{
    IDirect3DSurface9* surface = static_cast<Direct3DSurfaceContext*>(mid)->surface;
    surface->AddRef();
    *handle = surface;
    return MFX_ERR_NONE;
}

mfxStatus MFXDirect3DSurfaceAllocator::_free( mfxFrameAllocResponse* response )
{
    for( int i = 0; i < response->NumFrameActual; ++i )
    {
        Direct3DSurfaceContext* ctx = ((Direct3DSurfaceContext**)response->mids)[i];
        ctx->surface->Release();
        delete ctx;
    }
    delete[] (Direct3DSurfaceContext**)response->mids;

    return MFX_ERR_NONE;
}

bool MFXDirect3DSurfaceAllocator::initialize()
{
    m_direct3D9 = Direct3DCreate9( D3D_SDK_VERSION );
    if( !m_direct3D9 )
        return false;

    if( !openD3D9Device() )
        return false;
    m_prevOperationResult = m_d3d9manager->GetVideoService(
        m_d3DeviceHandle,
        IID_IDirectXVideoDecoderService,
        reinterpret_cast<void**>(&m_decoderService) );
    if( m_prevOperationResult != S_OK )
    {
        closeD3D9Device();
        return false;
    }

    m_initialized = true;
    return true;
}

bool MFXDirect3DSurfaceAllocator::initialized() const
{
    return m_initialized;
}

IDirect3DDevice9* MFXDirect3DSurfaceAllocator::d3d9Device() const
{
    return m_d3d9Device;
}

IDirect3DDeviceManager9* MFXDirect3DSurfaceAllocator::d3d9DeviceManager() const
{
    return m_d3d9manager;
}

HRESULT MFXDirect3DSurfaceAllocator::getLastError() const
{
    return m_prevOperationResult;
}

QString MFXDirect3DSurfaceAllocator::getLastErrorText() const
{
    return QString::fromWCharArray( DXGetErrorDescription( m_prevOperationResult ) );
}

static BOOL CALLBACK enumWindowsProc( HWND hwnd, LPARAM lParam )
{
    HWND* foundHwnd = (HWND*)lParam;
    DWORD processId = 0;
    GetWindowThreadProcessId( hwnd, &processId );
    if( processId == GetCurrentProcessId() )
    {
        *foundHwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

bool MFXDirect3DSurfaceAllocator::openD3D9Device()
{
    //searching for a window handle
    HWND foundHwnd = NULL;
    EnumWindows( enumWindowsProc, (LPARAM)&foundHwnd );
    if( foundHwnd == NULL )
        return false;   //could not find window

    //creating device
    D3DPRESENT_PARAMETERS presentationParameters;
    memset( &presentationParameters, 0, sizeof(presentationParameters) );
    presentationParameters.BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
    presentationParameters.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
    presentationParameters.BackBufferFormat = D3DFMT_X8R8G8B8;  //(D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' );
    presentationParameters.BackBufferCount = 1;
    presentationParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
    presentationParameters.MultiSampleQuality = 0;
    presentationParameters.SwapEffect = D3DSWAPEFFECT_COPY;
    presentationParameters.hDeviceWindow = foundHwnd;
    presentationParameters.Windowed = TRUE;
    presentationParameters.EnableAutoDepthStencil = FALSE;
    presentationParameters.Flags = D3DPRESENTFLAG_VIDEO;
    presentationParameters.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT; //since WINDOWED
    presentationParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    m_prevOperationResult = m_direct3D9->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        foundHwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &presentationParameters,
        &m_d3d9Device );
    if( m_prevOperationResult != S_OK )
        return false;

    //creating device manager
    m_prevOperationResult = DXVA2CreateDirect3DDeviceManager9( &m_deviceResetToken, &m_d3d9manager );
    if( m_prevOperationResult != S_OK )
    {
        m_d3d9Device->Release();
        m_d3d9Device = NULL;
        return false;
    }

    //resetting manager with device
    m_prevOperationResult = m_d3d9manager->ResetDevice( m_d3d9Device, m_deviceResetToken );
    if( m_prevOperationResult != S_OK )
    {
        m_d3d9manager->Release();
        m_d3d9manager = NULL;
        m_d3d9Device->Release();
        m_d3d9Device = NULL;
        return false;
    }

    m_prevOperationResult = m_d3d9manager->OpenDeviceHandle( &m_d3DeviceHandle );
    switch( m_prevOperationResult )
    {
        case S_OK:
            return true;

        default:
            m_d3d9manager->Release();
            m_d3d9manager = NULL;
            m_d3d9Device->Release();
            m_d3d9Device = NULL;
            return false;
    }
}

void MFXDirect3DSurfaceAllocator::closeD3D9Device()
{
    m_d3d9manager->CloseDeviceHandle( m_d3DeviceHandle );
    m_d3DeviceHandle = INVALID_HANDLE_VALUE;

    //releasing device manager
    m_d3d9manager->Release();
    m_d3d9manager = NULL;

    //closing device
    m_d3d9Device->Release();
    m_d3d9Device = NULL;
}

bool MFXDirect3DSurfaceAllocator::resetDevice()
{
    m_prevOperationResult = m_d3d9manager->ResetDevice( NULL, m_deviceResetToken );
    if( m_prevOperationResult != S_OK )
        return false;
    m_prevOperationResult = m_d3d9manager->OpenDeviceHandle( &m_d3DeviceHandle );
    if( m_prevOperationResult != S_OK )
        return false;

    return true;
}
