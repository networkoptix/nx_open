#ifdef _WIN32

#include "GL/glew.h"
#include "GL/wglew.h"
#include "GL/gl.h"

#include "directx_utils.h"

#include <array>

#include <nx/utils/log/log.h>
#include "device_handle.h"
#include "d3d_allocator.h"
#include "../quick_sync_surface.h"
#include "../quick_sync_video_decoder_impl.h"

#define WGL_ACCESS_READ_ONLY_NV 0x0000
#define WGL_ACCESS_READ_WRITE_NV 0x0001
#define WGL_ACCESS_WRITE_DISCARD_NV 0x0002

namespace nx::media::quick_sync {

namespace {
/*
typedef HANDLE (WINAPI * WGL_DX_OPEN_DEVICE_NV) (void* dxDevice);
typedef HANDLE (WINAPI * WGL_DX_REGISTER_OBJECT_NV) (HANDLE hDevice, void* dxObject, GLuint name, GLenum type, GLenum access);
typedef BOOL (WINAPI * WGL_DX_SET_RESOURCE_SHARE_HANDLE_NV) (void* dxObject, HANDLE shareHandle);
typedef BOOL (WINAPI * WGL_SWAP_INTERVAL_EXT) (int interval);
*/
int getIntelDeviceAdapter(MFXVideoSession& session)
{
    static const std::array<mfxIMPL, 4> impls = {
        MFX_IMPL_HARDWARE, MFX_IMPL_HARDWARE2, MFX_IMPL_HARDWARE3, MFX_IMPL_HARDWARE4};
    mfxIMPL impl;
    MFXQueryIMPL(session, &impl);
    mfxIMPL baseImpl = MFX_IMPL_BASETYPE(impl); // Extract Media SDK base implementation type

    // get corresponding adapter number
    for (int i = 0; i < impls.size(); i++)
    {
        if (impls[i] == baseImpl)
            return i;
    }
    NX_DEBUG(NX_SCOPE_TAG, "Failed to find adapter for session impl: %1", baseImpl);
    return -1;
}

} // namespace

bool Device::initialize(MFXVideoSession& session)
{
    auto adapter = getIntelDeviceAdapter(session);
    if (adapter < 0)
        return false;

    auto hr = device.CreateDevice(640, 640, adapter); // TODO size
    if (FAILED(hr))
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to get DirectX handle");
        return false;
    }

    auto status = session.SetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9,
        device.GetDeviceManager9());

    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to set DirectX handle to MFX session, error: %1", status);
        return false;
    }

    m_allocator = std::make_shared<windows::D3DFrameAllocator>();
    windows::D3DAllocatorParams d3dAllocParams;
    d3dAllocParams.pManager = device.GetDeviceManager9();
    status = m_allocator->Init(&d3dAllocParams);
    if (status < MFX_ERR_NONE)
    {
        m_allocator.reset();
        NX_ERROR(NX_SCOPE_TAG, "Failed to init D3D allocator, error: %1", status);
        return false;
    }
    return true;
}

std::shared_ptr<MFXFrameAllocator> Device::getAllocator()
{
    return m_allocator;
}

bool isCompatible(AVCodecID codec)
{
    if (codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_H265)
        return true;

    return false;
}

bool convertToRgb(
    IDirect3DDevice9Ex *pDevice, mfxFrameSurface1* mfxSurface, IDirect3DSurface9* pSharedSurface)
{
    mfxHDLPair* pair = (mfxHDLPair*)mfxSurface->Data.MemId;
    IDirect3DSurface9* decodedSurface = (IDirect3DSurface9*)pair->first;
    // Copy the result to the shared surface
    RECT rc = { 0, 0, mfxSurface->Info.Width, mfxSurface->Info.Height};
    HRESULT hr = pDevice->BeginScene();
    if (FAILED(hr))
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to convert to RGB, begin scene, error code: %1", hr);
        return false;
    }

    hr = pDevice->StretchRect(decodedSurface, &rc, pSharedSurface, &rc, D3DTEXF_NONE);
    if (FAILED(hr))
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to convert to RGB, stretch rect, error code: %1", hr);
        return false;
    }

    hr = pDevice->EndScene();
    if (FAILED(hr))
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to convert to RGB, end scene, error code: %1", hr);
        return false;
    }
    return true;
}

bool renderToRgb(const QVideoFrame& frame, bool isNewTexture, GLuint textureId)
{
    auto surfaceInfo = frame.handle().value<QuickSyncSurface>();
    auto decoderLock = surfaceInfo.decoder.lock();
    if (!decoderLock)
        return false;

    auto start =  std::chrono::high_resolution_clock::now();
    SimpleDXDevice& device = decoderLock->getDevice().device;

    IDirect3DSurface9* pRgbSurface = device.GetSharedSurface();

    if (!convertToRgb(device.GetDevice(), surfaceInfo.surface, pRgbSurface))
        return false;

    if (isNewTexture)
    {
        /*static PIXELFORMATDESCRIPTOR pfd =
        {
            sizeof(PIXELFORMATDESCRIPTOR),  1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA,
            32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0,
            PFD_MAIN_PLANE,
            0, 0, 0, 0
        };
        auto hDC = ::GetDC(device.getWindow());
        GLuint PixelFormat = ChoosePixelFormat(hDC, &pfd);
        SetPixelFormat(hDC, PixelFormat, &pfd);
        auto hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);
        
        GLenum err = glewInit();
        if (GLEW_OK != err)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Failed init glew: %1", glewGetErrorString(err));
            return false;
        }
        
        wglSwapIntervalEXT(0);
        wglMakeCurrent(NULL, NULL);
        wglMakeCurrent(hDC, hRC);*/

        
//         auto wglSwapIntervalEXT = (WGL_SWAP_INTERVAL_EXT)wglGetProcAddress("wglSwapIntervalEXT");
//         auto wglDXOpenDeviceNV = (WGL_DX_OPEN_DEVICE_NV)wglGetProcAddress("wglDXOpenDeviceNV");
//         auto wglDXRegisterObjectNV = (WGL_DX_REGISTER_OBJECT_NV)wglGetProcAddress("wglDXRegisterObjectNV");
//         auto wglDXSetResourceShareHandleNV =
//             (WGL_DX_SET_RESOURCE_SHARE_HANDLE_NV)wglGetProcAddress("wglDXSetResourceShareHandleNV");





        // Acquire a handle to the D3D device for use in OGL
        auto hDevice = wglDXOpenDeviceNV(device.GetDevice());
        if (!hDevice)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Failed open device NV");
            return false;
        }
        // This registers a resource that was created as shared in DX with its shared handle
        HANDLE hSurface = device.GetSharedHandle();
        BOOL success = wglDXSetResourceShareHandleNV(pRgbSurface, hSurface);

        // g_hTexture is the shared texture data, now identified by the texture name
        auto err0 = GetLastError();
        auto hTexture = wglDXRegisterObjectNV(
            hDevice, pRgbSurface, textureId, GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);
        auto err1 = GetLastError();
        if (hTexture == NULL)
            return false;
        wglMakeCurrent(NULL, NULL);
    }

    return true;
}

} // namespace nx::media::quick_sync

#endif // _WIN32
