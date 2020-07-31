#include "quick_sync_video_frame.h"

#include <nx/utils/log/log.h>

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

AVFrame QuickSyncVideoFrame::lockFrame()
{
    AVFrame result;
    memset(&result, 0, sizeof(AVFrame));
    auto surfaceInfo = m_frame->handle().value<QuickSyncSurface>();
    auto decoderLock = surfaceInfo.decoder.lock();
    if (!decoderLock)
    {
        NX_VERBOSE(this, "Quick sync decoder already deleted, failed to lock frame");
        return result;
    }

    mfxFrameSurface1* surface = surfaceInfo.surface;
    auto status = decoderLock->getDevice().getAllocator()->LockFrame(
        surface->Data.MemId, &surface->Data);
    if (MFX_ERR_NONE != status)
    {
        NX_ERROR(this, "Failed to lock video memory frame, error status: %1", toString(status));
        return result;
    }
    result.width = m_frame->width();
    result.height = m_frame->height();
    result.linesize[0] = surface->Data.PitchLow;
    result.linesize[1] = surface->Data.PitchLow;
    result.linesize[2] = surface->Data.PitchLow;
    result.linesize[3] = 0;
    result.data[0] = surface->Data.Y;
    result.data[1] = surface->Data.U;
    result.data[2] = surface->Data.V;
    result.data[3] = nullptr;
    result.format = AV_PIX_FMT_NV12;
    return result;
}

void QuickSyncVideoFrame::unlockFrame()
{
    AVFrame result;
    memset(&result, 0, sizeof(AVFrame));
    auto surfaceInfo = m_frame->handle().value<QuickSyncSurface>();
    auto decoderLock = surfaceInfo.decoder.lock();
    if (!decoderLock)
    {
        NX_VERBOSE(this, "Quick sync decoder already deleted, failed to unlock frame");
        return;
    }

    mfxFrameSurface1* surface = surfaceInfo.surface;
    auto status = decoderLock->getDevice().getAllocator()->UnlockFrame(
        surface->Data.MemId, &surface->Data);
}
