#pragma once

#ifdef __linux__

#include <memory>

#include <nx/media/quick_sync/quick_sync_video_decoder_impl.h>

#include <va/va.h>


struct VaSurfaceInfo
{
    VASurfaceID id;
    VADisplay display;
    mfxFrameSurface1* surface;

    // Contain reference to video decoder to ensure that it still alive
    std::weak_ptr<nx::media::quick_sync::QuickSyncVideoDecoderImpl> decoder;

    bool renderToRgb(bool isNewTexture, GLuint textureId);
};
Q_DECLARE_METATYPE(VaSurfaceInfo);

#endif // _linux__
