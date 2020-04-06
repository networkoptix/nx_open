#include "quick_sync_video_frame.h"

#include <nx/media/quick_sync/va_surface_info.h>

QuickSyncVideoFrame::QuickSyncVideoFrame(const std::shared_ptr<QVideoFrame>& frame)
{
    m_frame = frame;
}

bool QuickSyncVideoFrame::renderToRgb(bool isNewTexture, GLuint textureId)
{
    auto surfaceInfo = m_frame->handle().value<VaSurfaceInfo>();
    return surfaceInfo.renderToRgb(isNewTexture, textureId);
}
