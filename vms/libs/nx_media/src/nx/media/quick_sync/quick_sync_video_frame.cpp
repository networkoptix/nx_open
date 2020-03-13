#include "quick_sync_video_frame.h"

#include <nx/media/quick_sync/glx/va_glx.h>
#include <nx/media/quick_sync/va_surface_info.h>
#include <nx/media/quick_sync/quick_sync_video_decoder_impl.h>

QuickSyncVideoFrame::QuickSyncVideoFrame(
        const std::shared_ptr<QVideoFrame>& frame,
        std::weak_ptr<nx::media::QuickSyncVideoDecoderImpl> decoder)
{
    m_frame = frame;
    m_decoder = std::move(decoder);
}

bool QuickSyncVideoFrame::renderToRgb(bool isNewTexture, GLuint textureId)
{
    auto surfaceInfo = m_frame->handle().value<VaSurfaceInfo>();
    auto decoder = m_decoder.lock();
    if (!decoder)
        return false;

    VAStatus status;
    void** renderingSurface = decoder->getRenderingSurface();
    if (isNewTexture || !(*renderingSurface))
    {
        if (*renderingSurface)
            vaDestroySurfaceGLX(surfaceInfo.display, *renderingSurface);

        status = vaCreateSurfaceGLX_nx(
            surfaceInfo.display,
            GL_TEXTURE_2D,
            textureId,
            m_frame->size().width(), m_frame->size().height(),
            renderingSurface);
        if (status != VA_STATUS_SUCCESS)
        {
            NX_DEBUG(this, "vaCreateSurfaceGLX failed: %1", status);
            return false;
        }
    }
    status = vaCopySurfaceGLX_nx(surfaceInfo.display, *renderingSurface, surfaceInfo.id, 0);
    if (status != VA_STATUS_SUCCESS)
    {
        NX_DEBUG(this, "vaCopySurfaceGLX failed: %1", status);
        return false;
    }

    status = vaSyncSurface(surfaceInfo.display, surfaceInfo.id);
    if (status != VA_STATUS_SUCCESS)
    {
        NX_DEBUG(this, "vaSyncSurface failed: %1", status);
        return false;
    }
    return true;
}
