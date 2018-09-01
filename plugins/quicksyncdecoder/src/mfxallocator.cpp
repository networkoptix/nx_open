/**********************************************************
* 14 sep 2012
* a.kolesnikov
***********************************************************/

#include "mfxallocator.h"

#include <dxerr.h>

#include <utils/common/log.h>


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
    NX_VERBOSE(this, QString::fromLatin1("BaseMFXFrameAllocator(%1)::_free( %2 )").arg((size_t)this, 0, 16).arg((size_t)response, 0, 16));

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
    NX_VERBOSE(this, QString::fromLatin1("BaseMFXFrameAllocator(%1)::allocationResponseSuccessfullyProcessed").arg((size_t)this, 0, 16));

    AllocResponseCtx ctx;
    ctx.response = response;
    ctx.refCount = 1;
    ctx.allocType = request.Type;
    m_responses.push_back( ctx );
}

void BaseMFXFrameAllocator::releaseRemainingFrames()
{
    NX_VERBOSE(this, QString::fromLatin1("BaseMFXFrameAllocator(%1)::releaseRemainingFrames").arg((size_t)this, 0, 16));

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
    NX_VERBOSE(this, QString::fromLatin1("BaseMFXFrameAllocator(%1)::getCachedResponse( %2, %3 )").arg((size_t)this, 0, 16)
        .arg((size_t)request, 0, 16).arg((size_t)response, 0, 16));

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
    NX_VERBOSE(this, QString::fromLatin1("BaseMFXFrameAllocator(%1)::releaseFrameSurfaces( %2 )").arg((size_t)this, 0, 16).arg((size_t)response, 0, 16));

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
            mmid->width = ALIGN16_UP( request->Info.Width );
            mmid->height = ALIGN32_UP( request->Info.Height );
            mmid->chromaFormat = request->Info.ChromaFormat;
            mmid->fourCC = request->Info.FourCC;
            if( request->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV420 )
                mmid->base = (mfxU8*)_aligned_malloc( mmid->width*mmid->height*3/2, 128 );
            else if( request->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV422 )
                mmid->base = (mfxU8*)_aligned_malloc( mmid->width*mmid->height*2, 128 );
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
    _aligned_free( static_cast<SysMemoryFrameContext*>(frameCtx)->base );
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

MFXDirect3DSurfaceAllocator::MFXDirect3DSurfaceAllocator( IDirect3DDeviceManager9* d3d9manager, unsigned int adapterNumber )
:
    m_initialized( false ),
    m_d3d9manager( d3d9manager ),
    m_decoderService( NULL ),
    m_d3DeviceHandle( NULL ),
    m_deviceResetToken( 0 ),
    m_prevOperationResult( S_OK ),
    m_adapterNumber( adapterNumber )
{
    m_d3d9manager->AddRef();
}

MFXDirect3DSurfaceAllocator::~MFXDirect3DSurfaceAllocator()
{
    if( !m_initialized )
        return;

    releaseRemainingFrames();

    m_decoderService->Release();
    m_decoderService = NULL;

    if( m_d3DeviceHandle != NULL )
    {
        m_d3d9manager->CloseDeviceHandle( m_d3DeviceHandle );
        m_d3DeviceHandle = NULL;
    }

    m_d3d9manager->Release();
    m_d3d9manager = NULL;
}

mfxStatus MFXDirect3DSurfaceAllocator::alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response )
{
    //MFX_MEMTYPE_INTERNAL_FRAME, MFX_MEMTYPE_EXTERNAL_FRAME support:
        //for MFX_MEMTYPE_EXTERNAL_FRAME only increase ref count of allocation response (if already exists)

    if( !m_decoderService )
        return MFX_ERR_UNSUPPORTED;

    if( request->NumFrameSuggested == 0 )
        return MFX_ERR_NONE;

    if( (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) > 0 &&
        getCachedResponse( request, response ) ) //checking, whether such request has already been processed
    {
        //considering response with lower frame number as 
        //if( response->NumFrameActual < request->NumFrameMin )
        //    return MFX_ERR_MEMORY_ALLOC;
        return MFX_ERR_NONE;
    }

    DWORD renderTarget = 0;
    DWORD dxvaType = 0;
    if( request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET )
    {
        dxvaType = DXVA2_VideoDecoderRenderTarget;
    }
    else if( request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET )
    {
        dxvaType = DXVA2_VideoProcessorRenderTarget;
        //renderTarget = D3DUSAGE_RENDERTARGET;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    IDirect3DSurface9** surfaceArray = new IDirect3DSurface9*[request->NumFrameSuggested];

    for( int i = 0; i < 2; ++i )
    {
        if( request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET )
        {
            IDirect3DDevice9* d3d9Device = NULL;
            for( ;; )
            {
                m_prevOperationResult = m_d3d9manager->LockDevice( d3DeviceHandle(), &d3d9Device, TRUE );
                if( m_prevOperationResult == DXVA2_E_NEW_VIDEO_DEVICE )
                {
                    if( !reopenDeviceHandle() )
                        break;
                }
                else
                {
                    break;
                }
            }

            if( m_prevOperationResult != S_OK )
            {
                delete[] surfaceArray;
                return MFX_ERR_MEMORY_ALLOC;
            }

            for( int j = 0; j < request->NumFrameSuggested; ++j )
            {
                m_prevOperationResult = d3d9Device->CreateRenderTarget(
                    request->Info.Width,
                    request->Info.Height,
                    (D3DFORMAT)MAKEFOURCC('N','V','1','2'),
                    D3DMULTISAMPLE_NONE,
                    0,
                    TRUE,
                    &surfaceArray[j],
                    NULL );
                if( m_prevOperationResult != S_OK )
                    break;
            }

            d3d9Device->Release();
            m_d3d9manager->UnlockDevice( d3DeviceHandle(), FALSE );
        }
        else
        {
            m_prevOperationResult = m_decoderService->CreateSurface(
                request->Info.Width,
                request->Info.Height,
                request->NumFrameSuggested-1,   //CreateSurface creates BackBuffers+1 surfaces
                (D3DFORMAT)MAKEFOURCC('N','V','1','2'),
                D3DPOOL_DEFAULT,
                renderTarget,
                dxvaType,
                surfaceArray,
                NULL );
        }

        switch( m_prevOperationResult )
        {
            case S_OK:
                break;

            case E_FAIL:
                break;
                //if( resetDevice() )
                //    continue;

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
        NX_VERBOSE(this, QString::fromLatin1("Allocated IDirect3DSurface9 0x%1").arg((size_t)mids[i]->surface, 0, 16));
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
    //ULONG refCount = surface->AddRef();   //documentation says, we MUST do AddRef(), but Intel sample code does not do that...
    //NX_VERBOSE(this, QString::fromLatin1("MFXDirect3DSurfaceAllocator::gethdl. surf(%1) ref %2").arg((size_t)surface, 0, 16).arg(refCount));
    *handle = surface;
    return MFX_ERR_NONE;
}

bool MFXDirect3DSurfaceAllocator::initialize()
{
    if( !reopenDeviceHandle() )
        return false;

    m_prevOperationResult = m_d3d9manager->GetVideoService(
        m_d3DeviceHandle,
        IID_IDirectXVideoDecoderService,
        reinterpret_cast<void**>(&m_decoderService) );
    if( m_prevOperationResult != S_OK )
        return false;

    m_initialized = true;
    return true;
}

bool MFXDirect3DSurfaceAllocator::initialized() const
{
    return m_initialized;
}

unsigned int MFXDirect3DSurfaceAllocator::adapterNumber() const
{
    return m_adapterNumber;
}

IDirect3DDeviceManager9* MFXDirect3DSurfaceAllocator::d3d9DeviceManager() const
{
    return m_d3d9manager;
}

HANDLE MFXDirect3DSurfaceAllocator::d3DeviceHandle() const
{
    return m_d3DeviceHandle;
}

//!Opens new d3 device handle
bool MFXDirect3DSurfaceAllocator::reopenDeviceHandle()
{
    if( m_d3DeviceHandle != NULL )
    {
        m_d3d9manager->CloseDeviceHandle( m_d3DeviceHandle );
        m_d3DeviceHandle = NULL;
    }

    m_prevOperationResult = m_d3d9manager->OpenDeviceHandle( &m_d3DeviceHandle );
    return m_prevOperationResult == D3D_OK;
}

HRESULT MFXDirect3DSurfaceAllocator::getLastError() const
{
    return m_prevOperationResult;
}

QString MFXDirect3DSurfaceAllocator::getLastErrorText() const
{
    return QString::fromWCharArray( DXGetErrorDescription( m_prevOperationResult ) );
}

void MFXDirect3DSurfaceAllocator::deinitializeFrame( BaseFrameContext* const frameCtx )
{
    const ULONG refCount = static_cast<Direct3DSurfaceContext*>(frameCtx)->surface->Release();
    NX_VERBOSE(this, QString::fromLatin1("MFXDirect3DSurfaceAllocator::deinitializeFrame. surf(%1) ref %2").
        arg((size_t)static_cast<Direct3DSurfaceContext*>(frameCtx)->surface, 0, 16).arg(refCount));
}

//bool MFXDirect3DSurfaceAllocator::resetDevice()
//{
//    m_prevOperationResult = m_d3d9manager->ResetDevice( NULL, m_deviceResetToken );
//    return m_prevOperationResult == S_OK;
//}


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
