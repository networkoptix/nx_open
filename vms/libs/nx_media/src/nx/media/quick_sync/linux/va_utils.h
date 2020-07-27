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

namespace nx::media::quick_sync {

class Device
{
public:
    ~Device();
    bool initialize(MFXVideoSession& session);
    std::shared_ptr<MFXFrameAllocator> getAllocator();

    void* renderingSurface = nullptr;
    VADisplay display = nullptr;

private:
    std::shared_ptr<MFXFrameAllocator> m_allocator;
};

class QuickSyncVideoDecoderImpl;

bool isCompatible(AVCodecID codec);
bool renderToRgb(const QVideoFrame& frame, bool isNewTexture, GLuint textureId, QOpenGLContext* /*context*/);

} // nx::media::quick_sync

#endif // __linux__
