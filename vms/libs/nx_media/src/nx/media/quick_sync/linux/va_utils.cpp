#ifdef __linux__

#include "va_utils.h"

#include <nx/utils/log/log.h>

#include "glx/va_glx.h"
#include "va_display.h"
#include "../quick_sync_surface.h"
#include "../quick_sync_video_decoder_impl.h"

#include "vaapi_allocator.h"

namespace nx::media::quick_sync {

bool isCompatible(AVCodecID codec)
{
    if (linux::VaDisplay::getDisplay() == nullptr)
        return false;

    if (codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_H265)
        return true;

    return false;
}

DeviceContext::~DeviceContext()
{
    if (m_renderingSurface)
        vaDestroySurfaceGLX(linux::VaDisplay::getDisplay(), m_renderingSurface);
}

bool DeviceContext::initialize(MFXVideoSession& session)
{
    m_display = linux::VaDisplay::getDisplay();
    if (!m_display)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to get VA display");
        return false;
    }

    auto status = session.SetHandle(MFX_HANDLE_VA_DISPLAY, m_display);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to set VA handle to MFX session, error: %1", status);
        return false;
    }

    m_allocator = std::make_shared<linux::VaapiFrameAllocator>();
    linux::vaapiAllocatorParams vaapiAllocParams;
    vaapiAllocParams.m_dpy = m_display;
    status = m_allocator->Init(&vaapiAllocParams);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Failed to init VA allocator, error: %1", status);
        m_allocator.reset();
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
    QOpenGLContext* /*context*/)
{
    linux::vaapiMemId* surfaceData = (linux::vaapiMemId*)surfaceInfo.surface->Data.MemId;
    VASurfaceID surfaceId = *(surfaceData->m_surface);

    auto start =  std::chrono::high_resolution_clock::now();
    VAStatus status;
    if (isNewTexture || !m_renderingSurface)
    {
        if (m_renderingSurface)
            vaDestroySurfaceGLX(m_display, m_renderingSurface);

        status = vaCreateSurfaceGLX_nx(
            m_display,
            GL_TEXTURE_2D,
            textureId,
            surfaceInfo.surface->Info.Width, surfaceInfo.surface->Info.Height,
            &m_renderingSurface);
        if (status != VA_STATUS_SUCCESS)
        {
            NX_DEBUG(NX_SCOPE_TAG, "vaCreateSurfaceGLX failed: %1", status);
            return false;
        }
    }
    status = vaCopySurfaceGLX_nx(m_display, m_renderingSurface, surfaceId, 0);
    if (status != VA_STATUS_SUCCESS)
    {
        NX_DEBUG(NX_SCOPE_TAG, "vaCopySurfaceGLX failed: %1", status);
        return false;
    }

    status = vaSyncSurface(m_display, surfaceId);
    if (status != VA_STATUS_SUCCESS)
    {
        NX_DEBUG(NX_SCOPE_TAG, "vaSyncSurface failed: %1", status);
        return false;
    }
    auto end =  std::chrono::high_resolution_clock::now();
    NX_DEBUG(NX_SCOPE_TAG, "render time: %1", std::chrono::duration_cast<std::chrono::milliseconds>(end - start));
    return true;
}

} // namespace nx::media::quick_sync

#endif // __linux__
