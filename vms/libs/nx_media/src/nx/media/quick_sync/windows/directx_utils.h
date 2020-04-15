#pragma once

#ifdef _WIN32

#include <memory>

#include <mfx/mfxvideo++.h>
#include <QtMultimedia/QVideoFrame>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

#include "simpleDX9Device.h"

class MFXFrameAllocator;

namespace nx::media::quick_sync {

class QuickSyncVideoDecoderImpl;

struct Device
{
    bool initialize(MFXVideoSession& session);
    std::shared_ptr<MFXFrameAllocator> getAllocator();

    SimpleDXDevice device;
    std::shared_ptr<MFXFrameAllocator> m_allocator;
};

bool isCompatible(AVCodecID codec);
bool renderToRgb(const QVideoFrame& frame, bool isNewTexture, GLuint textureId);

} // nx::media::quick_sync

#endif // _WIN32
