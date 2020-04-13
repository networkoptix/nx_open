#pragma once

#ifdef __linux__

#include <memory>
#include <mfx/mfxvideo++.h>
#include <QtMultimedia/QVideoFrame>
#include <QtGui/QOpenGLFunctions>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

class MFXFrameAllocator;

namespace nx::media::quick_sync {

class QuickSyncVideoDecoderImpl;

bool setSessionHandle(MFXVideoSession& session);
bool isCompatible(AVCodecID codec);
bool renderToRgb(const QVideoFrame& frame, bool isNewTexture, GLuint textureId);

std::shared_ptr<MFXFrameAllocator> createVideoMemoryAllocator();
QAbstractVideoBuffer* createVideoBuffer(mfxFrameSurface1* surface, std::weak_ptr<QuickSyncVideoDecoderImpl> decoderPtr);

struct RenderingContext
{
    ~RenderingContext();
    void* renderingSurface = nullptr;
};

} // nx::media::quick_sync

#endif // _linux__
