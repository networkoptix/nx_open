#pragma once

#include <memory>

#include <QtMultimedia/QAbstractVideoBuffer>

#include <nx/media/quick_sync/quick_sync_video_decoder_impl.h>

#include <va/va.h>

constexpr int kHandleTypeVaSurface = QAbstractVideoBuffer::UserHandle + 1;

struct VaSurfaceInfo
{
    VASurfaceID id;
    VADisplay display;
    mfxFrameSurface1* surface;

    // Contain reference to video decoder to ensure that it still alive
    std::weak_ptr<nx::media::QuickSyncVideoDecoderImpl> decoder;

    bool renderToRgb(bool isNewTexture, GLuint textureId);
};
Q_DECLARE_METATYPE(VaSurfaceInfo);
