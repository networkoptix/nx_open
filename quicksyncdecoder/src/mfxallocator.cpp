/**********************************************************
* 14 sep 2012
* a.kolesnikov
***********************************************************/

#include "mfxallocator.h"

#include <dxerr.h>


bool operator==( const mfxFrameAllocResponse& left, const mfxFrameAllocResponse& right )
{
    return left.mids == right.mids;
}

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


BaseFrameContext::BaseFrameContext( const FrameMemoryType _type )
:
    type( _type ),
    width( 0 ),
    height( 0 )
{
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
// BaseMFXFrameAllocator
//////////////////////////////////////////////////////////
mfxStatus BaseMFXFrameAllocator::_free( mfxFrameAllocResponse* response )
{
    //checking, if we created this response
    std::list<AllocResponseCtx>::iterator responseIter = m_responses.begin();
    for( ;
        responseIter != m_responses.end();
        ++responseIter )
    {
        if( responseIter->response == *response )
            break;
    }

    if( responseIter == m_responses.end() )
        return MFX_ERR_INVALID_HANDLE;  //we didn't create this response, doing nothing...

    //releasing surfaces...
    --responseIter->refCount;
    if( responseIter->refCount == 0 )
    {
        releaseFrameSurfaces( &responseIter->response );
        *response = responseIter->response;
        m_responses.erase( responseIter );
    }

    return MFX_ERR_NONE;
}

void BaseMFXFrameAllocator::allocationResponseSuccessfullyProcessed(
    const mfxFrameAllocRequest& request,
    const mfxFrameAllocResponse& response )
{
    AllocResponseCtx ctx;
    ctx.response = response;
    ctx.refCount = 1;
    ctx.allocType = request.Type;
    m_responses.push_back( ctx );
}

void BaseMFXFrameAllocator::releaseRemainingFrames()
{
    for( std::list<AllocResponseCtx>::iterator
        it = m_responses.begin();
        it != m_responses.end();
         )
    {
        it->refCount = 1;   //ignoring refcount, since MUST release all resources
        releaseFrameSurfaces( &it->response );
        m_responses.erase( it++ );
    }
}

bool BaseMFXFrameAllocator::getCachedResponse( mfxFrameAllocRequest* const request, mfxFrameAllocResponse* const response )
{
    for( std::list<AllocResponseCtx>::iterator
        it = m_responses.begin();
        it != m_responses.end();
        ++it )
    {
        if( it->allocType == request->Type )
        {
            *response = it->response;
            ++it->refCount;
            return true;
        }
    }

    return false;
}

void BaseMFXFrameAllocator::releaseFrameSurfaces( mfxFrameAllocResponse* const response )
{
    for( int i = 0; i < response->NumFrameActual; ++i )
    {
        BaseFrameContext* ctx = ((BaseFrameContext**)response->mids)[i];
        deinitializeFrame( ctx );
        delete ctx;
    }
    delete[] (BaseFrameContext**)response->mids;
    response->NumFrameActual = 0;
}


//////////////////////////////////////////////////////////
// MFXSysMemFrameAllocator (example from Intel Media SDK )
//////////////////////////////////////////////////////////
MFXSysMemFrameAllocator::SysMemoryFrameContext::SysMemoryFrameContext()
:
    BaseFrameContext( BaseFrameContext::sysMem ),
    base( NULL ),
    chromaFormat( 0 ),
    fourCC( 0 )
{
}

MFXSysMemFrameAllocator::~MFXSysMemFrameAllocator()
{
    releaseRemainingFrames();
}

mfxStatus MFXSysMemFrameAllocator::alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response )
{
    if( !(request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) )
        return MFX_ERR_UNSUPPORTED;
    if( (request->Info.FourCC != MFX_FOURCC_NV12) && (request->Info.FourCC != MFX_FOURCC_YV12) )
        return MFX_ERR_UNSUPPORTED;
    //response->mids = (mfxMemId*)malloc( request->NumFrameSuggested * sizeof(SysMemoryFrameContext*) );
    response->mids = (mfxMemId*)(new SysMemoryFrameContext*[request->NumFrameSuggested]);
    response->NumFrameActual = 0;
    try
    {
        for( int i=0; i<request->NumFrameSuggested; ++i )
        {
            SysMemoryFrameContext* mmid = new SysMemoryFrameContext();
            mmid->width = ALIGN16( request->Info.Width );
            mmid->height = ALIGN32( request->Info.Height );
            mmid->chromaFormat = request->Info.ChromaFormat;
            mmid->fourCC = request->Info.FourCC;
            if( request->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV420 )
                mmid->base = (mfxU8*)malloc(mmid->width*mmid->height*3/2);
            else if( request->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV422 )
                mmid->base = (mfxU8*)malloc(mmid->width*mmid->height*2);
            response->mids[i] = mmid;
            ++response->NumFrameActual;
        }

        allocationResponseSuccessfullyProcessed( *request, *response );
    }
    catch( ... )
    {
        //cleaning up...
        for( int i=0; i<response->NumFrameActual; ++i )
            delete reinterpret_cast<SysMemoryFrameContext*>(response->mids[i]);
        delete[] reinterpret_cast<SysMemoryFrameContext**>(response->mids);
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXSysMemFrameAllocator::lock( mfxMemId mid, mfxFrameData* ptr )
{
    //works only for YUV
    SysMemoryFrameContext* mmid = (SysMemoryFrameContext*)mid;
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

mfxStatus MFXSysMemFrameAllocator::unlock( mfxMemId /*mid*/, mfxFrameData* ptr )
{
    if( ptr )
        ptr->Y = ptr->U = ptr->V = ptr->A = NULL;
    return MFX_ERR_NONE;
}

mfxStatus MFXSysMemFrameAllocator::gethdl( mfxMemId /*mid*/, mfxHDL* /*handle*/ )
{
    return MFX_ERR_UNSUPPORTED;
}

void MFXSysMemFrameAllocator::deinitializeFrame( BaseFrameContext* const frameCtx )
{
    free( static_cast<SysMemoryFrameContext*>(frameCtx)->base );
}


//////////////////////////////////////////////////////////
// MFXDirect3DSurfaceAllocator
//////////////////////////////////////////////////////////
MFXDirect3DSurfaceAllocator::Direct3DSurfaceContext::Direct3DSurfaceContext()
:
    BaseFrameContext( BaseFrameContext::dxva2 ),
    surface( NULL )
{
}

MFXDirect3DSurfaceAllocator::MFXDirect3DSurfaceAllocator( IDirect3D9Ex* direct3D9, unsigned int adapterNumber )
:
    m_initialized( false ),
    m_direct3D9( direct3D9 ),
    m_d3d9manager( NULL ),
    m_d3d9Device( NULL ),
    m_decoderService( NULL ),
    m_deviceResetToken( 0 ),
    m_prevOperationResult( S_OK ),
    m_adapterNumber( adapterNumber ),
    m_dc( NULL )
{
    //memset( &m_prevResponse, 0, sizeof(m_prevResponse) );
}

MFXDirect3DSurfaceAllocator::~MFXDirect3DSurfaceAllocator()
{
    if( !m_initialized )
        return;

    releaseRemainingFrames();

    m_decoderService->Release();
    m_decoderService = NULL;

    closeD3D9Device();
}

mfxStatus MFXDirect3DSurfaceAllocator::alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response )
{
    //MFX_MEMTYPE_INTERNAL_FRAME, MFX_MEMTYPE_EXTERNAL_FRAME support:
        //for MFX_MEMTYPE_EXTERNAL_FRAME only increase ref count of allocation response (if already exists)

    if( (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) > 0 &&
        getCachedResponse( request, response ) ) //checking, whether such request has already been processed
    {
        if( response->NumFrameActual < request->NumFrameMin )
        {
            releaseFrameSurfaces( response );
            return MFX_ERR_MEMORY_ALLOC;
        }
        return MFX_ERR_NONE;
    }

    DWORD renderTarget = 0;
    if( request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET )
        renderTarget = DXVA2_VideoDecoderRenderTarget;
    else if( request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET )
        renderTarget = DXVA2_VideoProcessorRenderTarget;
    else
        return MFX_ERR_UNSUPPORTED;

    IDirect3DSurface9** surfaceArray = new IDirect3DSurface9*[request->NumFrameSuggested];

    for( int i = 0; i < 2; ++i )
    {
        m_prevOperationResult = m_decoderService->CreateSurface(
            request->Info.Width,
            request->Info.Height,
            request->NumFrameSuggested-1,   //CreateSurface creates BackBuffers+1 surfaces
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

    Direct3DSurfaceContext** mids = new Direct3DSurfaceContext*[request->NumFrameSuggested];
    for( int i = 0; i < request->NumFrameSuggested; ++i )
    {
        mids[i] = new Direct3DSurfaceContext();
        mids[i]->surface = surfaceArray[i];
        mids[i]->width = request->Info.Width;
        mids[i]->height = request->Info.Height;
    }
    response->mids = reinterpret_cast<mfxMemId*>(mids);
    response->NumFrameActual = request->NumFrameSuggested;
    delete[] surfaceArray;

    allocationResponseSuccessfullyProcessed( *request, *response );

    return MFX_ERR_NONE;
}

mfxStatus MFXDirect3DSurfaceAllocator::lock( mfxMemId mid, mfxFrameData* ptr )
{
    D3DLOCKED_RECT lockedRect; 
    memset( &lockedRect, 0, sizeof(lockedRect) );
    Direct3DSurfaceContext* surfaceCtx = static_cast<Direct3DSurfaceContext*>(mid);
    if( surfaceCtx->surface->LockRect( &lockedRect, NULL, D3DLOCK_NOSYSLOCK ) != D3D_OK )
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
    //surface->AddRef();
    *handle = surface;
    return MFX_ERR_NONE;
}

bool MFXDirect3DSurfaceAllocator::initialize()
{
    if( !openD3D9Device() )
        return false;

    HANDLE d3DeviceHandle = INVALID_HANDLE_VALUE;
    m_prevOperationResult = m_d3d9manager->OpenDeviceHandle( &d3DeviceHandle );
    if( m_prevOperationResult != S_OK )
    {
        closeD3D9Device();
        return false;
    }

    m_prevOperationResult = m_d3d9manager->GetVideoService(
        d3DeviceHandle,
        IID_IDirectXVideoDecoderService,
        reinterpret_cast<void**>(&m_decoderService) );
    m_d3d9manager->CloseDeviceHandle( d3DeviceHandle ); //closing device handle in any case
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

IDirect3DDevice9Ex* MFXDirect3DSurfaceAllocator::d3d9Device() const
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

HDC MFXDirect3DSurfaceAllocator::dc() const
{
    return m_dc;
}

void MFXDirect3DSurfaceAllocator::deinitializeFrame( BaseFrameContext* const frameCtx )
{
    static_cast<Direct3DSurfaceContext*>(frameCtx)->surface->Release();
}

//const mfxFrameAllocResponse& MFXDirect3DSurfaceAllocator::prevResponse() const
//{
//    return m_prevResponse;
//}
//
//void MFXDirect3DSurfaceAllocator::clearPrevResponse()
//{
//    memset( &m_prevResponse, 0, sizeof(m_prevResponse) );
//}

static BOOL CALLBACK enumWindowsProc( HWND hwnd, LPARAM lParam )
{
    //HWND* foundHwnd = (HWND*)lParam;
    std::list<HWND>* processTopLevelWindows = (std::list<HWND>*)lParam;

    DWORD processId = 0;
    GetWindowThreadProcessId( hwnd, &processId );
    if( processId == GetCurrentProcessId() )
    {
        //*foundHwnd = hwnd;
        //return FALSE;
        //HWND parentWnd = GetAncestor( hwnd, GA_PARENT );
        processTopLevelWindows->push_back( hwnd );
    }
    return TRUE;
}

bool MFXDirect3DSurfaceAllocator::openD3D9Device()
{
    //searching for a window handle
    std::list<HWND> processTopLevelWindows;
    HWND processWindow = NULL;
    EnumWindows( enumWindowsProc, (LPARAM)&processTopLevelWindows );
    if( processTopLevelWindows.empty() )
        return false;   //could not find window
    processWindow = processTopLevelWindows.front();
    m_dc = GetDC( processWindow );

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
    m_prevOperationResult = m_direct3D9->CreateDeviceEx(
        m_adapterNumber,
        D3DDEVTYPE_HAL,
        processWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &presentationParameters,
        NULL,
        &m_d3d9Device );
    if( m_prevOperationResult != S_OK )
        return false;

    m_prevOperationResult = m_d3d9Device->ResetEx( &presentationParameters, NULL );
    if( m_prevOperationResult != S_OK )
    {
        m_d3d9Device->Release();
        m_d3d9Device = NULL;
        return false;
    }

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

    return true;
}

void MFXDirect3DSurfaceAllocator::closeD3D9Device()
{
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
    return m_prevOperationResult == S_OK;
}


//////////////////////////////////////////////////////////
// MFXFrameAllocatorDispatcher
//////////////////////////////////////////////////////////
MFXFrameAllocatorDispatcher::MFXFrameAllocatorDispatcher(
    MFXSysMemFrameAllocator* const sysMemFrameAllocator,
    MFXDirect3DSurfaceAllocator* const d3dFrameAllocator )
:
    m_sysMemFrameAllocator( sysMemFrameAllocator ),
    m_d3dFrameAllocator( d3dFrameAllocator )
{
}

mfxStatus MFXFrameAllocatorDispatcher::alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response )
{
    if( request->Type & MFX_MEMTYPE_SYSTEM_MEMORY )
        return m_sysMemFrameAllocator->alloc( request, response );
    else if( request->Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET) )
        return m_d3dFrameAllocator->alloc( request, response );
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXFrameAllocatorDispatcher::lock( mfxMemId mid, mfxFrameData* ptr )
{
    switch( static_cast<BaseFrameContext*>(mid)->type )
    {
        case BaseFrameContext::sysMem: 
            return m_sysMemFrameAllocator->lock( mid, ptr );
        case BaseFrameContext::dxva2:
            return m_d3dFrameAllocator->lock( mid, ptr );
        default:
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
}

mfxStatus MFXFrameAllocatorDispatcher::unlock( mfxMemId mid, mfxFrameData* ptr )
{
    switch( static_cast<BaseFrameContext*>(mid)->type )
    {
        case BaseFrameContext::sysMem: 
            return m_sysMemFrameAllocator->unlock( mid, ptr );
        case BaseFrameContext::dxva2:
            return m_d3dFrameAllocator->unlock( mid, ptr );
        default:
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
}

mfxStatus MFXFrameAllocatorDispatcher::gethdl( mfxMemId mid, mfxHDL* handle )
{
    switch( static_cast<BaseFrameContext*>(mid)->type )
    {
        case BaseFrameContext::sysMem: 
            return m_sysMemFrameAllocator->gethdl( mid, handle );
        case BaseFrameContext::dxva2:
            return m_d3dFrameAllocator->gethdl( mid, handle );
        default:
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
}

mfxStatus MFXFrameAllocatorDispatcher::_free( mfxFrameAllocResponse* response )
{
    if( response->NumFrameActual == 0 )
        return MFX_ERR_NONE;

    switch( static_cast<BaseFrameContext*>(response->mids[0])->type )
    {
        case BaseFrameContext::sysMem: 
            return m_sysMemFrameAllocator->_free( response );
        case BaseFrameContext::dxva2:
            return m_d3dFrameAllocator->_free( response );
        default:
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
}
