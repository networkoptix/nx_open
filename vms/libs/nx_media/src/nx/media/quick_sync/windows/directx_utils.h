#pragma once

#ifdef _WIN32

#include <memory>

#include <mfx/mfxvideo++.h>
#include <QtMultimedia/QVideoFrame>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

#include "device_handle.h"

class MFXFrameAllocator;
struct QuickSyncSurface;

namespace nx::media::quick_sync {

class QuickSyncVideoDecoderImpl;

class DeviceContext
{
public:
    bool initialize(MFXVideoSession& session, int width, int height);
    bool renderToRgb(
        const QuickSyncSurface& surfaceInfo,
        bool isNewTexture,
        GLuint textureId,
        QOpenGLContext* context);

    std::shared_ptr<MFXFrameAllocator> getAllocator();

private:
    windows::DeviceHandle m_device;
    std::shared_ptr<MFXFrameAllocator> m_allocator;
};

bool isCompatible(AVCodecID codec);

} // nx::media::quick_sync

#endif // _WIN32
