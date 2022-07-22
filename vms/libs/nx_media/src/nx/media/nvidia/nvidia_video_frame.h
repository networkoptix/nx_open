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
    bool renderToRgb(GLuint textureId, int textureWidth, int textureHeight);
    virtual AVFrame lockFrame() override;
    virtual void unlockFrame() override;

public:
    int width = 0;
    int height = 0;
    int64_t timestamp = 0;
    std::weak_ptr<nx::media::nvidia::NvidiaVideoDecoder> decoder;

    uint8_t* frameData = nullptr;
    int format;
    int bitDepth;
    int matrix;
};
