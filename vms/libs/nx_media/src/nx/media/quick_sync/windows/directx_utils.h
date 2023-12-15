// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef _WIN32

#include <memory>

#include <QtMultimedia/QVideoFrame>

#include <vpl/mfx.h>
#include <vpl/mfxvideo++.h>

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
        const mfxFrameSurface1* surface,
        bool isNewTexture,
        GLuint textureId,
        QOpenGLContext* context);

    std::shared_ptr<MFXFrameAllocator> getAllocator();

private:
    windows::DeviceHandle m_device;
    std::shared_ptr<MFXFrameAllocator> m_allocator;
};

} // nx::media::quick_sync

#endif // _WIN32
