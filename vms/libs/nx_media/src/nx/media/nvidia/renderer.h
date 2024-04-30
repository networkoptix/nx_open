// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <cuda.h>

#include <QtCore/QSize>
#include <QtGui/QOpenGLFunctions>

namespace nx::media::nvidia {

class NvidiaVideoFrame;

class NX_MEDIA_API Renderer : public QOpenGLFunctions
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

    /** Buffer to upload the scaled frame. */
    CUdeviceptr m_scaledFrame = 0;

    CUgraphicsResource m_cuResource = nullptr;

    /** Buffer object to upload the texture data. */
    GLuint m_pbo = 0;
};

} // nx::media::nvidia
