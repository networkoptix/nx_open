// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifdef __linux__

#include "va_utils.h"

#include <nx/utils/log/log.h>

#include "glx/va_glx.h"
#include "va_display.h"
#include "../quick_sync_surface.h"
#include "../quick_sync_video_decoder_impl.h"

#include "vaapi_allocator.h"

namespace nx::media::quick_sync {

DeviceContext::~DeviceContext()
{
    if (m_renderingSurface)
        vaDestroySurfaceGLX(linux::VaDisplay::getDisplay(), m_renderingSurface);
}

bool DeviceContext::initialize(MFXVideoSession& session, int /*width*/, int /*height*/)
{
    m_display = linux::VaDisplay::getDisplay();
    if (!m_display)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to get VA display");
        return false;
    }

    auto status = session.SetHandle(MFX_HANDLE_VA_DISPLAY, m_display);
    if (status < MFX_ERR_NONE)
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to set VA handle to MFX session, error: %1", status);
        return false;
    }

    m_allocator = std::make_shared<linux::VaapiFrameAllocator>(m_display);
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
    QOpenGLContext* /*context*/)
{
    linux::vaapiMemId* surfaceData = (linux::vaapiMemId*)surface->Data.MemId;
    VASurfaceID surfaceId = *(surfaceData->m_surface);

    VAStatus status;
    QSize sourceSize(surface->Info.Width, surface->Info.Height);
    if (isNewTexture || !m_renderingSurface || m_renderingSurfaceSize != sourceSize)
    {
        NX_DEBUG(NX_SCOPE_TAG, "CreateSurfaceGLX size %1", sourceSize);

        if (m_renderingSurface)
            vaDestroySurfaceGLX(linux::VaDisplay::getDisplay(), m_renderingSurface);

        status = vaCreateSurfaceGLX_nx(
            m_display,
            GL_TEXTURE_2D,
            textureId,
            surface->Info.Width, surface->Info.Height,
            &m_renderingSurface);
        if (status != VA_STATUS_SUCCESS)
        {
            NX_WARNING(NX_SCOPE_TAG, "vaCreateSurfaceGLX failed: %1", status);
            return false;
        }
        m_renderingSurfaceSize = sourceSize;
    }
    status = vaCopySurfaceGLX_nx(m_display, m_renderingSurface, surfaceId, 0);
    if (status != VA_STATUS_SUCCESS)
    {
        NX_WARNING(NX_SCOPE_TAG, "vaCopySurfaceGLX failed: %1", status);
        return false;
    }

    status = vaSyncSurface(m_display, surfaceId);
    if (status != VA_STATUS_SUCCESS)
    {
        NX_WARNING(NX_SCOPE_TAG, "vaSyncSurface failed: %1", status);
        return false;
    }
    return true;
}

} // namespace nx::media::quick_sync

#endif // __linux__
