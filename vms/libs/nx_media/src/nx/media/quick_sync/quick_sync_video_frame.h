#pragma once

#include <utils/media/frame_info.h>

class QuickSyncVideoFrame : public AbstractVideoSurface
{
public:
    QuickSyncVideoFrame(
        const std::shared_ptr<QVideoFrame>& frame,
        std::weak_ptr<nx::media::QuickSyncVideoDecoderImpl> decoder);

    virtual bool renderToRgb(bool isNewTexture, GLuint textureId) override;

//private:
    // Contain video surface
    std::shared_ptr<QVideoFrame> m_frame;
    // Contain reference to video decoder to ensure that it still alive
    std::weak_ptr<nx::media::QuickSyncVideoDecoderImpl> m_decoder;
};
