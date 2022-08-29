// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef __linux__

#include <QtCore/QSize>
#include <QtGui/QOpenGLFunctions>

#include <memory>
#include <cuda.h>

namespace nx::media::nvidia {

class NvidiaVideoFrame;

}

namespace nx::media::nvidia::linux {

class Renderer : public QOpenGLFunctions
{
public:
    ~Renderer();
    bool render(GLuint textureId, QSize textureSize, const NvidiaVideoFrame& frame);

private:
    bool resize(const NvidiaVideoFrame& frame, const QSize& dstSize);
    bool convertToRgb(uint8_t* frameData, const QSize& size, int pitch, int format, int bitDepth, int matrix);
    bool draw(GLuint textureId, int width, int height);
    bool initializePbo(int width, int height);
    bool initializeScaledFrame(int width, int height);
    void freePbo();
    void freeScaledFrame();

    QSize m_textureSize;
    CUdeviceptr m_scaledFrame = 0; //< Buffer to upload scaled frame
    CUgraphicsResource m_cuResource;
    GLuint m_pbo = 0; //< Buffer object to upload texture data
};

} // nx::media::nvidia::linux

#endif // __linux__
