#ifdef _WIN32

#include "directx_utils.h"
#include <nx/utils/log/log.h>

#include "d3d_allocator.h"

namespace nx::media::quick_sync {

bool setSessionHandle(MFXVideoSession& /*session*/)
{
   /* auto display = VaDisplay::getDisplay();
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
    }*/

    return true;
}

bool isCompatible(AVCodecID codec)
{
    /*if (VaDisplay::getDisplay() == nullptr)
        return false;
*/
    if (codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_H265)
        return true;

    return false;
}

std::shared_ptr<MFXFrameAllocator> createVideoMemoryAllocator()
{
    auto allocator = std::make_shared<D3DFrameAllocator>();
    D3DAllocatorParams d3dAllocParams;
    d3dAllocParams.pManager = reinterpret_cast<IDirect3DDeviceManager9 *>(handle);
    auto status = allocator->Init(&d3dAllocParams);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to init D3D allocator, error: %1", status);
        return nullptr;
    }
    return allocator;
}

QAbstractVideoBuffer* createVideoBuffer(mfxFrameSurface1* surface, std::weak_ptr<QuickSyncVideoDecoderImpl> decoderPtr)
{
    vaapiMemId* surfaceData = (vaapiMemId*)surface->Data.MemId;
    VASurfaceID surfaceId = *(surfaceData->m_surface);
    VaSurfaceInfo surfaceInfo {surfaceId, VaDisplay::getDisplay(), surface, std::move(decoderPtr)};
    return new VaQtVideoBuffer(surfaceInfo);
}

RenderingContext::~RenderingContext()
{
    if (renderingSurface)
        vaDestroySurfaceGLX(VaDisplay::getDisplay(), renderingSurface);
}

bool renderToRgb(const QVideoFrame& frame, bool isNewTexture, GLuint textureId)
{
    auto surfaceInfo = frame.handle().value<VaSurfaceInfo>();
    return surfaceInfo.renderToRgb(isNewTexture, textureId);
}

} // namespace nx::media::quick_sync

#endif // _WIN32
