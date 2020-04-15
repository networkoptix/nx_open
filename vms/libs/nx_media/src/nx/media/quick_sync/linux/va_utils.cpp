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

Device::~Device()
{
    if (renderingSurface)
        vaDestroySurfaceGLX(linux::VaDisplay::getDisplay(), renderingSurface);
}

bool Device::initialize(MFXVideoSession& session)
{
    display = linux::VaDisplay::getDisplay();
    if (!display)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to get VA display");
        return false;
    }

    auto status = session.SetHandle(MFX_HANDLE_VA_DISPLAY, display);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to set VA handle to MFX session, error: %1", status);
        return false;
    }

    m_allocator = std::make_shared<linux::VaapiFrameAllocator>();
    linux::vaapiAllocatorParams vaapiAllocParams;
    vaapiAllocParams.m_dpy = display;
    status = m_allocator->Init(&vaapiAllocParams);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Failed to init VA allocator, error: %1", status);
        m_allocator.reset();
        return false;
    }
    return true;
}

std::shared_ptr<MFXFrameAllocator> Device::getAllocator()
{
    return m_allocator;
}

bool renderToRgb(const QVideoFrame& frame, bool isNewTexture, GLuint textureId)
{
    auto surfaceInfo = frame.handle().value<QuickSyncSurface>();
    linux::vaapiMemId* surfaceData = (linux::vaapiMemId*)surfaceInfo.surface->Data.MemId;
    VASurfaceID surfaceId = *(surfaceData->m_surface);

    auto decoderLock = surfaceInfo.decoder.lock();
    if (!decoderLock)
        return false;

    auto start =  std::chrono::high_resolution_clock::now();
    Device& device = decoderLock->getDevice();
    VAStatus status;
    if (isNewTexture || !device.renderingSurface)
    {
        if (device.renderingSurface)
            vaDestroySurfaceGLX(device.display, device.renderingSurface);

        status = vaCreateSurfaceGLX_nx(
            device.display,
            GL_TEXTURE_2D,
            textureId,
            surfaceInfo.surface->Info.Width, surfaceInfo.surface->Info.Height,
            &device.renderingSurface);
        if (status != VA_STATUS_SUCCESS)
        {
            NX_DEBUG(NX_SCOPE_TAG, "vaCreateSurfaceGLX failed: %1", status);
            return false;
        }
    }
    status = vaCopySurfaceGLX_nx(device.display, device.renderingSurface, surfaceId, 0);
    if (status != VA_STATUS_SUCCESS)
    {
        NX_DEBUG(NX_SCOPE_TAG, "vaCopySurfaceGLX failed: %1", status);
        return false;
    }

    status = vaSyncSurface(device.display, surfaceId);
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
