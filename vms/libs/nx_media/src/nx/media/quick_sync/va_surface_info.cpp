#include "va_surface_info.h"

#include <nx/media/quick_sync/glx/va_glx.h>

bool VaSurfaceInfo::renderToRgb(bool isNewTexture, GLuint textureId)
{
    //auto start =  std::chrono::high_resolution_clock::now();
    auto decoderLock = decoder.lock();
    if (!decoderLock)
        return false;

    VAStatus status;
    void** renderingSurface = decoderLock->getRenderingSurface();
    if (isNewTexture || !(*renderingSurface))
    {
        if (*renderingSurface)
            vaDestroySurfaceGLX(display, *renderingSurface);

        status = vaCreateSurfaceGLX_nx(
            display,
            GL_TEXTURE_2D,
            textureId,
            surface->Info.Width, surface->Info.Height,
            renderingSurface);
        if (status != VA_STATUS_SUCCESS)
        {
            NX_DEBUG(this, "vaCreateSurfaceGLX failed: %1", status);
            return false;
        }
    }
    status = vaCopySurfaceGLX_nx(display, *renderingSurface, id, 0);
    if (status != VA_STATUS_SUCCESS)
    {
        NX_DEBUG(this, "vaCopySurfaceGLX failed: %1", status);
        return false;
    }

    status = vaSyncSurface(display, id);
    if (status != VA_STATUS_SUCCESS)
    {
        NX_DEBUG(this, "vaSyncSurface failed: %1", status);
        return false;
    }
    //auto end =  std::chrono::high_resolution_clock::now();
    //NX_DEBUG(this, "render time: %1", std::chrono::duration_cast<std::chrono::milliseconds>(end - start));
    return true;
}
