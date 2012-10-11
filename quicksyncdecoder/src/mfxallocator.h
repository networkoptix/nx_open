/**********************************************************
* 14 sep 2012
* a.kolesnikov
***********************************************************/

#ifndef MFXALLOCATOR_H
#define MFXALLOCATOR_H

#include <list>

#include <QString>

#include <mfxvideo.h>

#include <initguid.h>
#include <D3D9.h>
#include <Dxva2api.h>


template<class Numeric> Numeric ALIGN16( Numeric n )
{
    Numeric tmp = n & 0x0F;
    return tmp == 0 ? n : (n - tmp + 16);
}

template<class Numeric> Numeric ALIGN32( Numeric n )
{
    Numeric tmp = n & 0x1F;
    return tmp == 0 ? n : (n - tmp + 32);
}

//!Routes calls to func from \a mfxBufferAllocator to implementation of abstract methods
class AbstractMFXBufferAllocator
:
    public mfxBufferAllocator
{
public:
    AbstractMFXBufferAllocator();
    virtual ~AbstractMFXBufferAllocator();

    virtual mfxStatus alloc( mfxU32 nbytes, mfxU16 /*type*/, mfxMemId* mid ) = 0;
    virtual mfxStatus lock( mfxMemId mid, mfxU8 **ptr ) = 0;
    virtual mfxStatus unlock( mfxMemId /*mid*/ ) = 0;
    virtual mfxStatus _free( mfxMemId mid ) = 0;

    static mfxStatus ba_alloc( mfxHDL /*pthis*/, mfxU32 nbytes, mfxU16 /*type*/, mfxMemId* mid );
    static mfxStatus ba_lock( mfxHDL /*pthis*/, mfxMemId mid, mfxU8 **ptr );
    static mfxStatus ba_unlock(mfxHDL /*pthis*/, mfxMemId /*mid*/);
    static mfxStatus ba_free( mfxHDL /*pthis*/, mfxMemId mid );
};

class MFXBufferAllocator
:
    public AbstractMFXBufferAllocator
{
public:
    virtual mfxStatus alloc( mfxU32 nbytes, mfxU16 /*type*/, mfxMemId* mid );
    virtual mfxStatus lock( mfxMemId mid, mfxU8 **ptr );
    virtual mfxStatus unlock( mfxMemId /*mid*/ );
    virtual mfxStatus _free( mfxMemId mid );
};

//!Frame, allocated by MFX allocator. Allocators MUST subclass this one
class BaseFrameContext
{
public:
    enum FrameMemoryType
    {
        sysMem,
        dxva2
    };

    const FrameMemoryType type;
    //!these values are always aligned to 32-byte boundary top
    mfxU16 width;
    mfxU16 height;

    BaseFrameContext( const FrameMemoryType _type );
    virtual ~BaseFrameContext() {}
};

//!Routes calls to func from \a mfxFrameAllocator to implementation of abstract methods
/*!
    \note Implementation is not required to be thread-safe
*/
class AbstractMFXFrameAllocator
:
    public mfxFrameAllocator
{
public:
    AbstractMFXFrameAllocator();
    virtual ~AbstractMFXFrameAllocator();

    virtual mfxStatus alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response ) = 0;
    virtual mfxStatus lock( mfxMemId mid, mfxFrameData* ptr ) = 0;
    virtual mfxStatus unlock( mfxMemId /*mid*/, mfxFrameData* ptr ) = 0;
    virtual mfxStatus gethdl( mfxMemId /*mid*/, mfxHDL* /*handle*/ ) = 0;
    virtual mfxStatus _free( mfxFrameAllocResponse* response ) = 0;

    static mfxStatus fa_alloc( mfxHDL /*pthis*/, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response );
    static mfxStatus fa_lock( mfxHDL /*pthis*/, mfxMemId mid, mfxFrameData* ptr );
    static mfxStatus fa_unlock( mfxHDL /*pthis*/, mfxMemId /*mid*/, mfxFrameData* ptr );
    static mfxStatus fa_gethdl( mfxHDL /*pthis*/, mfxMemId /*mid*/, mfxHDL* /*handle*/ );
    static mfxStatus fa_free( mfxHDL /*pthis*/, mfxFrameAllocResponse* response );
};

//!Implements common logic for all frame allocators
/*!
    Holds reference counter on all allocation responses. Reference counter incremented in \a getCachedResponse, decremented in \a releaseFrameSurfaces

    Implementation of this class MUST call \a allocationResponseSuccessfullyProcessed after successfully processing \a alloc request 
    and call \a releaseRemainingFrames() in destructor

    \note Subclass MUST allocate mmid array with \a new[], each mmid element MUST be BaseFrameContext - subclass, allocated with \a new
*/
class BaseMFXFrameAllocator
:
    public AbstractMFXFrameAllocator
{
public:
    //!Implementation of AbstractMFXFrameAllocator::_free
    /*!
        Subclass does not have to reimplement this method, but only implement \a deinitializeFrame
    */
    virtual mfxStatus _free( mfxFrameAllocResponse* response );

protected:
    //!New alloc request has been processed successfully
    void allocationResponseSuccessfullyProcessed(
        const mfxFrameAllocRequest& request,
        const mfxFrameAllocResponse& response );
    //!Sub class MUST call this method to destroy remaining frames
    /*!
        It cannot be called from \a ~BaseMFXFrameAllocator, since it has to call \a deinitializeFrame implementation
    */
    void releaseRemainingFrames();
    //!Searches for cached response for request \a request. If found, increases refcount and returns true
    bool getCachedResponse( mfxFrameAllocRequest* const request, mfxFrameAllocResponse* const response );

    //!Deinitializes frame, must not release memory \a frameCtx!
    virtual void deinitializeFrame( BaseFrameContext* const frameCtx ) = 0;
    void releaseFrameSurfaces( mfxFrameAllocResponse* const response );

private:
    struct AllocResponseCtx
    {
        mfxFrameAllocResponse response;
        int refCount;
        int allocType;

        AllocResponseCtx()
        :
            refCount( 1 ),
            allocType( 0 )
        {
        }
    };

    //!list<pair<response, ref_count> >stores each allocation response
    std::list<AllocResponseCtx> m_responses;
};

//!Allocates frame buffers on system memory
/*!
    \note Supports only NV12 color format
*/
class MFXSysMemFrameAllocator
:
    public BaseMFXFrameAllocator
{
public:
    ~MFXSysMemFrameAllocator();

    virtual mfxStatus alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response );
    virtual mfxStatus lock( mfxMemId mid, mfxFrameData* ptr );
    virtual mfxStatus unlock( mfxMemId mid, mfxFrameData* ptr );
    virtual mfxStatus gethdl( mfxMemId mid, mfxHDL* handle );

protected:
    virtual void deinitializeFrame( BaseFrameContext* const frameCtx );

private:
    //!Frame, allocated in system memory
    class SysMemoryFrameContext
    :
        public BaseFrameContext
    {
    public:
        mfxU8* base;
        mfxU16 chromaFormat;
        mfxU32 fourCC;

        SysMemoryFrameContext();
    };
};

//!Allocates frame as IDirect3DSurface9 (data stored in video memory)
/*!
    After class instantiation one MUST call \a initialize to create D3D9 device and get neccessary handles
    \note Supports only NV12 color format
*/
class MFXDirect3DSurfaceAllocator
:
    public BaseMFXFrameAllocator
{
public:
    /*!
        \param adapterNumber Graphics adapter to use (to create D3D device on). By default, default adapter is used
    */
    MFXDirect3DSurfaceAllocator( IDirect3D9Ex* direct3D9, unsigned int adapterNumber = 0 );
    virtual ~MFXDirect3DSurfaceAllocator();

    virtual mfxStatus alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response );
    virtual mfxStatus lock( mfxMemId mid, mfxFrameData* ptr );
    virtual mfxStatus unlock( mfxMemId mid, mfxFrameData* ptr );
    virtual mfxStatus gethdl( mfxMemId mid, mfxHDL* handle );

    /*!
        Creates D3D9 device
    */
    bool initialize();
    //!Returns true if D3D9 device was created and initialized successfully
    bool initialized() const;
    //!Return value is not NULL only if \a initialized() returns \a true
    IDirect3DDevice9Ex* d3d9Device() const;
    //!Return value is not NULL only if \a initialized() returns \a true
    IDirect3DDeviceManager9* d3d9DeviceManager() const;
    HRESULT getLastError() const;
    QString getLastErrorText() const;
    HDC dc() const;

    //!Returns response from previous \a alloc call
    //const mfxFrameAllocResponse& prevResponse() const;
    //void clearPrevResponse();

protected:
    virtual void deinitializeFrame( BaseFrameContext* const frameCtx );

private:
    struct Direct3DSurfaceContext
    :
        public BaseFrameContext
    {
    public:
        IDirect3DSurface9* surface;

        Direct3DSurfaceContext();
    };

    bool m_initialized;
    IDirect3D9Ex* m_direct3D9;
    IDirect3DDeviceManager9* m_d3d9manager;
    IDirect3DDevice9Ex* m_d3d9Device;
    IDirectXVideoDecoderService* m_decoderService;
    UINT m_deviceResetToken;
    HRESULT m_prevOperationResult;
    unsigned int m_adapterNumber;
    HDC m_dc;

    bool openD3D9Device();
    void closeD3D9Device();
    bool resetDevice();
};

//!Redirects calls to \a MFXDirect3DSurfaceAllocator or \a MFXSysMemFrameAllocator depending on requested memory type
class MFXFrameAllocatorDispatcher
:
    public AbstractMFXFrameAllocator
{
public:
    /*!
        \note Does not own allocators \a sysMemFrameAllocator and \a d3dFrameAllocator
    */
    MFXFrameAllocatorDispatcher(
        MFXSysMemFrameAllocator* const sysMemFrameAllocator,
        MFXDirect3DSurfaceAllocator* const d3dFrameAllocator );

    virtual mfxStatus alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response );
    virtual mfxStatus lock( mfxMemId mid, mfxFrameData* ptr );
    virtual mfxStatus unlock( mfxMemId mid, mfxFrameData* ptr );
    virtual mfxStatus gethdl( mfxMemId mid, mfxHDL* handle );
    virtual mfxStatus _free( mfxFrameAllocResponse* response );

private:
    MFXSysMemFrameAllocator* const m_sysMemFrameAllocator;
    MFXDirect3DSurfaceAllocator* const m_d3dFrameAllocator;
};

#endif  //MFXALLOCATOR_H
