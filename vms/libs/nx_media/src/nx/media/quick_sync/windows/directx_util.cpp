#ifdef _WIN32

#include "GL/glew.h"
#include "GL/wglew.h"
#include "GL/gl.h"

#include "directx_utils.h"

#include <array>

#include <nx/utils/log/log.h>
#include "d3d_allocator.h"
#include "../quick_sync_surface.h"

namespace {

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

namespace nx::media::quick_sync {

bool isCompatible(AVCodecID codec)
{
    if (codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_H265)
        return true;

    return false;
}

bool DeviceContext::initialize(MFXVideoSession& session)
{
    auto adapter = getIntelDeviceAdapter(session);
    if (adapter < 0)
        return false;

    auto hr = m_device.createDevice(2992, 2992, adapter); // TODO size
    if (FAILED(hr))
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to get DirectX handle");
        return false;
    }

    auto status = session.SetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9,
        m_device.getDeviceManager());

    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to set DirectX handle to MFX session, error: %1", status);
        return false;
    }

    m_allocator = std::make_shared<windows::D3DFrameAllocator>();
    windows::D3DAllocatorParams d3dAllocParams;
    d3dAllocParams.pManager = m_device.getDeviceManager();
    status = m_allocator->Init(&d3dAllocParams);
    if (status < MFX_ERR_NONE)
    {
        m_allocator.reset();
        NX_ERROR(NX_SCOPE_TAG, "Failed to init D3D allocator, error: %1", status);
        return false;
    }
    return true;
}

std::shared_ptr<MFXFrameAllocator> DeviceContext::getAllocator()
{
    return m_allocator;
}

bool DeviceContext::renderToRgb(
    const QuickSyncSurface& surfaceInfo,
    bool isNewTexture,
    GLuint textureId,
    QOpenGLContext* context)
{
    return m_device.getRenderer().render(surfaceInfo.surface, isNewTexture, textureId, context);
}

} // namespace nx::media::quick_sync

#endif // _WIN32
