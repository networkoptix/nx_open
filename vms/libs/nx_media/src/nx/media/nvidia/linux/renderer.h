// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef __linux__

#include <memory>
#include <GL/gl.h>
#include <nvcuvid.h>

namespace nx::media::nvidia::linux {

class RendererImpl;

class Renderer
{
public:
    Renderer();
    ~Renderer();
    bool initialize(int width, int height, int pitch);
    bool render(GLuint textureId, uint8_t* frameData, cudaVideoSurfaceFormat format, int bitDepth, int matrix, int textureWidth, int textureHeight);

private:
    std::unique_ptr<RendererImpl> m_impl;
};

} // nx::media::quick_sync::linux

#endif // __linux__
