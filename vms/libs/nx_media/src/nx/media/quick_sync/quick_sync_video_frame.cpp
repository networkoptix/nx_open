#include "quick_sync_video_frame.h"

#include <nx/media/quick_sync/utils.h>
#include <nx/media/quick_sync/quick_sync_surface.h>
#include <nx/media/quick_sync/quick_sync_video_decoder_impl.h>

QuickSyncVideoFrame::QuickSyncVideoFrame(const std::shared_ptr<QVideoFrame>& frame)
{
    m_frame = frame;
}

bool QuickSyncVideoFrame::renderToRgb(bool isNewTexture, GLuint textureId, QOpenGLContext* context)
{
    auto surfaceInfo = m_frame->handle().value<QuickSyncSurface>();
    auto decoderLock = surfaceInfo.decoder.lock();
    if (!decoderLock)
        return false;

    return decoderLock->getDevice().renderToRgb(surfaceInfo, isNewTexture, textureId, context);
}
