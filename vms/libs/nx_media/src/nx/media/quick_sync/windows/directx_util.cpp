// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifdef _WIN32

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

bool DeviceContext::initialize(MFXVideoSession& session, int width, int height)
{
    auto adapter = getIntelDeviceAdapter(session);
    if (adapter < 0)
        return false;

    // The decoder shares openGL texture between QS and mainWindows GL context.
    // Currently it is able to obtain QS data for default adapter only.
    if (adapter != 0)
        return false;

    if (!m_device.createDevice(width, height, adapter))
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

    m_allocator = std::make_shared<windows::D3DFrameAllocator>(m_device.getDeviceManager());
    return true;
}

std::shared_ptr<MFXFrameAllocator> DeviceContext::getAllocator()
{
    return m_allocator;
}

bool DeviceContext::renderToRgb(
    const mfxFrameSurface1* surface,
    bool isNewTexture,
    GLuint textureId,
    QOpenGLContext* context)
{
    return m_device.getRenderer().render(surface, isNewTexture, textureId, context);
}

} // namespace nx::media::quick_sync

#endif // _WIN32
