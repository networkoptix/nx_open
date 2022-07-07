// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utils/media/frame_info.h>

#include <QtGui/QOpenGLContext>
#include <QtMultimedia/QVideoFrame>

namespace nx::media::quick_sync { class QuickSyncVideoDecoderImpl; }

class QuickSyncVideoFrame: public AbstractVideoSurface
{
public:
    QuickSyncVideoFrame(
        const std::shared_ptr<QVideoFrame>& frame,
        std::weak_ptr<nx::media::quick_sync::QuickSyncVideoDecoderImpl> decoder);

    bool renderToRgb(
        bool isNewTexture,
        GLuint textureId,
        QOpenGLContext* context,
        int scaleFactor,
        float* cropWidth,
        float* cropHeight);
    virtual AVFrame lockFrame() override;
    virtual void unlockFrame() override;
    virtual SurfaceType type() override { return SurfaceType::Intel; }

private:
    // Contain video surface
    std::shared_ptr<QVideoFrame> m_frame;
    std::weak_ptr<nx::media::quick_sync::QuickSyncVideoDecoderImpl> m_decoder;
};

inline bool renderToRgb(
    AbstractVideoSurface* frame,
    bool isNewTexture,
    GLuint textureId,
    QOpenGLContext* context,
    int scaleFactor,
    float* cropWidth,
    float* cropHeight)
{
    QuickSyncVideoFrame* qsvFrame = dynamic_cast<QuickSyncVideoFrame*>(frame);
    if (!qsvFrame)
        return false;

    return qsvFrame->renderToRgb(isNewTexture, textureId, context, scaleFactor, cropWidth, cropHeight);
}
