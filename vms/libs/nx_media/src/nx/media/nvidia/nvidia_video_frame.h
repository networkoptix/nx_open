// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QOpenGLContext>
#include <QtMultimedia/QVideoFrame>

#include <utils/media/frame_info.h>

namespace nx::media::nvidia {
class NvidiaVideoDecoder;
}

class NvidiaVideoFrame: public AbstractVideoSurface
{
public:
    bool renderToRgb(
        bool isNewTexture,
        GLuint textureId,
        int scaleFactor,
        float* cropWidth,
        float* cropHeight);
    virtual AVFrame lockFrame() override;
    virtual void unlockFrame() override;

public:
    uint8_t* frameData;
    int format;
    int bitDepth = 0;
    int matrix = 0;
    int width = 0;
    int height = 0;
    std::weak_ptr<nx::media::nvidia::NvidiaVideoDecoder> decoder;
};


