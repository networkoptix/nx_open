#pragma once

#ifdef __linux__

#include <memory>
#include <mfx/mfxvideo++.h>
#include <QtMultimedia/QVideoFrame>
#include <QtGui/QOpenGLFunctions>
#include <va/va.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

class MFXFrameAllocator;
struct QuickSyncSurface;

namespace nx::media::quick_sync {

class DeviceContext
{
public:
    ~DeviceContext();
    bool initialize(MFXVideoSession& session, int width, int height);
    bool renderToRgb(
        const mfxFrameSurface1* surface,
        bool isNewTexture,
        GLuint textureId,
        QOpenGLContext* /*context*/);

    std::shared_ptr<MFXFrameAllocator> getAllocator();

private:
    void* m_renderingSurface = nullptr;
    QSize m_renderingSurfaceSize;
    VADisplay m_display = nullptr;
    std::shared_ptr<MFXFrameAllocator> m_allocator;
};

} // nx::media::quick_sync

#endif // __linux__
