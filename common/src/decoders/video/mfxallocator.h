/**********************************************************
* 14 sep 2012
* a.kolesnikov
***********************************************************/

#ifndef MFXALLOCATOR_H
#define MFXALLOCATOR_H

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

//!Routes calls to func from \a mfxFrameAllocator to implementation of abstract methods
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

//!Allocates frame buffers on system memory
/*!
    \note Supports only NV12 color format
*/
class MFXFrameAllocator
:
    public AbstractMFXFrameAllocator
{
public:
    virtual mfxStatus alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response );
    virtual mfxStatus lock( mfxMemId mid, mfxFrameData* ptr );
    virtual mfxStatus unlock( mfxMemId /*mid*/, mfxFrameData* ptr );
    virtual mfxStatus gethdl( mfxMemId /*mid*/, mfxHDL* /*handle*/ );
    virtual mfxStatus _free( mfxFrameAllocResponse* response );
};

//!Allocates frame as IDirect3DSurface9 (data stored in video memory)
/*!
    After class instantiation one MUST call \a initialize to create D3D9 device and get neccessary handles
    \note Supports only NV12 color format
*/
class MFXDirect3DSurfaceAllocator
:
    public AbstractMFXFrameAllocator
{
public:
    MFXDirect3DSurfaceAllocator();
    virtual ~MFXDirect3DSurfaceAllocator();
    
    virtual mfxStatus alloc( mfxFrameAllocRequest* request, mfxFrameAllocResponse* response );
    virtual mfxStatus lock( mfxMemId mid, mfxFrameData* ptr );
    virtual mfxStatus unlock( mfxMemId /*mid*/, mfxFrameData* ptr );
    virtual mfxStatus gethdl( mfxMemId /*mid*/, mfxHDL* /*handle*/ );
    virtual mfxStatus _free( mfxFrameAllocResponse* response );

    /*!
        Creates D3D9 device
    */
    bool initialize();
    //!Returns true if D3D9 device was created and initialized successfully
    bool initialized() const;
    //!Return value is not NULL only if \a initialized() returns \a true
    IDirect3DDevice9* d3d9Device() const;
    //!Return value is not NULL only if \a initialized() returns \a true
    IDirect3DDeviceManager9* d3d9DeviceManager() const;
    HRESULT getLastError() const;
    QString getLastErrorText() const;

private:
    struct Direct3DSurfaceContext
    {
    public:
        IDirect3DSurface9* surface;
        int width;
        int height;

        Direct3DSurfaceContext();
    };

    bool m_initialized;
    IDirect3D9* m_direct3D9;
    IDirect3DDeviceManager9* m_d3d9manager;
    IDirect3DDevice9* m_d3d9Device;
    HANDLE m_d3DeviceHandle;
    IDirectXVideoDecoderService* m_decoderService;
    UINT m_deviceResetToken;
    HRESULT m_prevOperationResult;

    bool openD3D9Device();
    void closeD3D9Device();
    bool resetDevice();
};

#endif  //MFXALLOCATOR_H
