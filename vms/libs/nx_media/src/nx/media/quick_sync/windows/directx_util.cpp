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

bool renderToRgb(const QVideoFrame& frame, bool isNewTexture, GLuint textureId, QOpenGLContext* context)
{
    auto surfaceInfo = frame.handle().value<QuickSyncSurface>();
    auto decoderLock = surfaceInfo.decoder.lock();
    if (!decoderLock)
        return false;

    if (!decoderLock->getDevice().device.getRenderer().render(surfaceInfo.surface, isNewTexture, textureId, context))
    {
        return false;
    }

    return true;
}

} // namespace nx::media::quick_sync

#endif // _WIN32
