#pragma once

#include <mfx/mfxvideo.h>
#include <QtMultimedia/QAbstractVideoBuffer>

constexpr int kHandleTypeQsvSurface = QAbstractVideoBuffer::UserHandle + 1;

namespace nx::media::quick_sync {
class QuickSyncVideoDecoderImpl;
}

struct QuickSyncSurface
{
    mfxFrameSurface1* surface;
    // Contain reference to video decoder to ensure that it still alive
    std::weak_ptr<nx::media::quick_sync::QuickSyncVideoDecoderImpl> decoder;
};
Q_DECLARE_METATYPE(QuickSyncSurface);

