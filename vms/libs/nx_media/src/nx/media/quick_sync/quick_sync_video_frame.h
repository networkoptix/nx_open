#pragma once

#include <utils/media/frame_info.h>

#include <QtGui/QOpenGLContext>
#include <QtMultimedia/QVideoFrame>

class QuickSyncVideoFrame: public AbstractVideoSurface
{
public:
    QuickSyncVideoFrame(const std::shared_ptr<QVideoFrame>& frame);

    bool renderToRgb(
        bool isNewTexture,
        GLuint textureId,
        QOpenGLContext* context,
        int scaleFactor,
        float* cropWidth,
        float* cropHeight);
    virtual AVFrame lockFrame() override;
    virtual void unlockFrame() override;
    virtual QSize size() override;

private:
    // Contain video surface
    std::shared_ptr<QVideoFrame> m_frame;
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
