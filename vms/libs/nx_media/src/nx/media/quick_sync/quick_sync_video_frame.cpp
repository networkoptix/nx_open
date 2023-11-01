// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "quick_sync_video_frame.h"

#include <nx/media/quick_sync/quick_sync_surface.h>
#include <nx/media/quick_sync/quick_sync_video_decoder_impl.h>
#include <nx/media/quick_sync/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>

#include "mfx_sys_qt_video_buffer.h"
#include "qt_video_buffer.h"

namespace {

mfxFrameSurface1* getSurface(const nx::media::VideoFramePtr& frame)
{
    QAbstractVideoBuffer* videoBuffer = frame->videoBuffer();

    NX_ASSERT(dynamic_cast<nx::media::quick_sync::QtVideoBuffer*>(videoBuffer)
        || dynamic_cast<nx::media::quick_sync::MfxQtVideoBuffer*>(videoBuffer));

    return reinterpret_cast<mfxFrameSurface1*>(videoBuffer->textureHandle(/*unused*/ 0));
}

} // namespace

QuickSyncVideoFrame::QuickSyncVideoFrame(
    const nx::media::VideoFramePtr& frame,
    std::weak_ptr<nx::media::quick_sync::QuickSyncVideoDecoderImpl> decoder)
{
    m_frame = frame;
    m_decoder = decoder;
}

bool QuickSyncVideoFrame::renderToRgb(
    bool isNewTexture,
    GLuint textureId,
    QOpenGLContext* context,
    [[maybe_unused]] int scaleFactor,
    float* cropWidth,
    float* cropHeight)
{
    auto surface = getSurface(m_frame);

    auto decoderLock = m_decoder.lock();
    if (!decoderLock)
        return false;

    mfxFrameSurface1* scaledSurface = surface;

#ifndef Q_OS_LINUX // Linux version is faster with downscale in vaCopySurfaceGLX_nx call
    if (scaleFactor != 1)
    {
        QSize sourceSize(surface->Info.Width, surface->Info.Height);
        QSize targetSize = sourceSize / scaleFactor;

        if (!decoderLock->scaleFrame(surface, &scaledSurface, targetSize))
        {
            NX_WARNING(this, "Failed to scale video surface");
            return false;
        }
    }
#endif
    mfxFrameInfo info = scaledSurface->Info;
    if (cropWidth)
        *cropWidth = info.CropW / (float) info.Width;
    if (cropHeight)
        *cropHeight = info.CropH / (float) info.Height;
    return decoderLock->getDevice().renderToRgb(scaledSurface, isNewTexture, textureId, context);
}

AVFrame QuickSyncVideoFrame::lockFrame()
{
    AVFrame result;
    memset(&result, 0, sizeof(AVFrame));

    auto decoderLock = m_decoder.lock();
    if (!decoderLock)
    {
        NX_DEBUG(this, "Quick sync decoder already deleted, failed to lock frame");
        return result;
    }

    mfxFrameSurface1* surface = getSurface(m_frame);
    auto status = decoderLock->getDevice().getAllocator()->LockFrame(
        surface->Data.MemId, &surface->Data);
    if (MFX_ERR_NONE != status)
    {
        NX_ERROR(this, "Failed to lock video memory frame, error: %1", status);
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
    auto decoderLock = m_decoder.lock();
    if (!decoderLock)
    {
        NX_VERBOSE(this, "Quick sync decoder already deleted, failed to unlock frame");
        return;
    }

    mfxFrameSurface1* surface = getSurface(m_frame);
    auto status = decoderLock->getDevice().getAllocator()->UnlockFrame(
        surface->Data.MemId, &surface->Data);
    if (MFX_ERR_NONE != status)
        NX_DEBUG(this, "Failed to unlock video memory frame, error: %1", status);
}
