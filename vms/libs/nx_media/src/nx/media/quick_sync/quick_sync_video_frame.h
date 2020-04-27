#pragma once

#include <utils/media/frame_info.h>

class QuickSyncVideoFrame : public AbstractVideoSurface
{
public:
    QuickSyncVideoFrame(const std::shared_ptr<QVideoFrame>& frame);

    virtual bool renderToRgb(bool isNewTexture, GLuint textureId, QOpenGLContext* context) override;

private:
    // Contain video surface
    std::shared_ptr<QVideoFrame> m_frame;
};
