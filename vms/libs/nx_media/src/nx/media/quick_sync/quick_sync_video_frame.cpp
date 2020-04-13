#include "quick_sync_video_frame.h"

#include <nx/media/quick_sync/utils.h>

QuickSyncVideoFrame::QuickSyncVideoFrame(const std::shared_ptr<QVideoFrame>& frame)
{
    m_frame = frame;
}

bool QuickSyncVideoFrame::renderToRgb(bool isNewTexture, GLuint textureId)
{
    return nx::media::quick_sync::renderToRgb(*m_frame, isNewTexture, textureId);
}
