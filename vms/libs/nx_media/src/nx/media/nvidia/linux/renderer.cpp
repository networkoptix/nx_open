// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/
#ifdef __linux__

#include <nx/utils/log/log.h>

#include <GL/glew.h>

#include "renderer.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <cuda.h>
#include <cudaGL.h>

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>

#include <nx/media/nvidia/nvidia_utils.h>
#include <nx/media/nvidia/NvCodecUtils.h>
#include <nx/media/nvidia/ColorSpace.h>


namespace nx::media::nvidia::linux {

class RendererImpl
{
public:
    ~RendererImpl();
    bool initialize(int width, int height);
    bool convertToRgb(uint8_t* frameData, int width, int height, cudaVideoSurfaceFormat format, int bitDepth, int matrix);
    bool render(GLuint textureId, int width, int height);
    bool resize(uint8_t* frameData, int pitch, int width, int height, int srcWidth, int srcHeight, int bitDepth);
    bool initResizedBuffer(int width, int height);

    int m_width = 0;
    int m_height = 0;
    int m_pitch = 0;

    int m_resizedWidth = 0;
    int m_resizedHeight = 0;

    CUgraphicsResource m_cuResource;
    CUdeviceptr m_resizedFrame = 0;
    GLuint m_pbo = 0; //< Buffer object to upload texture data
};

RendererImpl::~RendererImpl()
{
    cuGraphicsUnregisterResource(m_cuResource);

    // Release OpenGL resources.
    glDeleteBuffers(1, &m_pbo);
}

 bool RendererImpl::initResizedBuffer(int width, int height)
{
    int size = width * (height + height / 2);
    auto status = cuMemAlloc(&m_resizedFrame, size);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to alloc buffer: %1", toString(status));
        return false;
    }
    status = cuMemsetD8(m_resizedFrame, 0, size);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to memset buffer: %1", toString(status));
        return false;
    }
    return true;
}

bool RendererImpl::initialize(int width, int height)
{
    NX_DEBUG(this, "Render init: %1x%2", width, height);
    if (m_pbo)
    {
        cuGraphicsUnregisterResource(m_cuResource);
        // Release OpenGL resources.
        glDeleteBuffers(1, &m_pbo);
        m_pbo = 0;
    }

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        NX_WARNING(this, "glewInit failed: %1", glewGetErrorString(err));
        return false;
    }

    glGenBuffers(1, &m_pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * 4, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // Attach pbo to the cuda graphics resource
    auto status = cuGraphicsGLRegisterBuffer(
        &m_cuResource, m_pbo, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to attach pbo: %1", toString(status));
        return false;
    }

    return true;
}

bool RendererImpl::resize(uint8_t* frameData, int pitch, int width, int height, int srcWidth, int srcHeight, int bitDepth)
{
    if (!m_resizedFrame || m_resizedWidth != width || m_resizedHeight != height)
    {
        initialize(width, height);
        if (!initResizedBuffer(width, height))
            return false;
        m_resizedWidth = width;
        m_resizedHeight = height;
    }
    if (bitDepth == 8)
        ResizeNv12((uint8_t*)m_resizedFrame, width, width, height, frameData, pitch, srcWidth, srcHeight);
    else
        ResizeP016((uint8_t*)m_resizedFrame, width, width, height, frameData, pitch, srcWidth, srcHeight);
    return true;
}

bool RendererImpl::convertToRgb(
    uint8_t* frameData, int width, int
    height, cudaVideoSurfaceFormat format, int bitDepth, int matrix)
{
    int pitch = width * 4;

    auto status = cuGraphicsMapResources(1, &m_cuResource, 0);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to map resource: %1", toString(status));
        return false;
    }

    size_t bufferSize = 0;
    CUdeviceptr deviceBuffer;
    status = cuGraphicsResourceGetMappedPointer(&deviceBuffer, &bufferSize, m_cuResource);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to get mapped pointer: %1", toString(status));
        return false;
    }

    // Launch cuda kernels for colorspace conversion from raw video to raw image formats which
    // OpenGL textures can work with
    if (bitDepth == 8)
    {
        if (format == cudaVideoSurfaceFormat_YUV444)
            YUV444ToColor32<BGRA32>(frameData, width, (uint8_t*)deviceBuffer, pitch, width, height, matrix);
        else // default assumed NV12
            Nv12ToColor32<BGRA32>(frameData, width, (uint8_t*)deviceBuffer, pitch, width, height, matrix);
    }
    else
    {
        if (format == cudaVideoSurfaceFormat_YUV444)
            YUV444P16ToColor32<BGRA32>(frameData, 2 * width, (uint8_t*)deviceBuffer, pitch, width, height, matrix);
        else // default assumed P016
            P016ToColor32<BGRA32>(frameData, 2 * width, (uint8_t*)deviceBuffer, pitch, width, height, matrix);
    }

    status = cuGraphicsUnmapResources(1, &m_cuResource, 0);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to unmap resource: %1", toString(status));
        return false;
    }
    return true;
}

QString glErrorString(GLenum err)
{
    switch (err)
    {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";

        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";

        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";

        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";

        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW";

        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW";

        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";

        case GL_TABLE_TOO_LARGE:
            return "GL_TABLE_TOO_LARGE";

        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";

        default:
            return QString("UNKNOWN 0x%1").arg(err, 0, 16);
    }
}

bool RendererImpl::render(GLuint textureId, int width, int height)
{
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
    auto error = glGetError();
    if (error != GL_NO_ERROR)
        NX_WARNING(NX_SCOPE_TAG, "OpenGL error in glBindBuffer: %1, pbo: %2", glErrorString(error), m_pbo);

    glBindTexture(GL_TEXTURE_2D, textureId);
    error = glGetError();
    if (error != GL_NO_ERROR)
        NX_WARNING(NX_SCOPE_TAG, "OpenGL error in glBindTexture: %1, textureId: %2", glErrorString(error), textureId);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, 0);
    error = glGetError();
    if (error != GL_NO_ERROR)
        NX_WARNING(NX_SCOPE_TAG, "OpenGL error in glTexSubImage2D: %1, textureId: %2, pbo: %3", glErrorString(error), textureId, m_pbo);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

Renderer::Renderer()
{

}

Renderer::~Renderer()
{

}

bool Renderer::initialize(int width, int height, int pitch)
{
    m_impl = std::make_unique<RendererImpl>();
    m_impl->m_width = width;
    m_impl->m_height = height;
    m_impl->m_pitch = pitch;
    return true;
}

bool Renderer::render(GLuint textureId, uint8_t* frameData, cudaVideoSurfaceFormat format, int bitDepth, int matrix, int textureWidth, int textureHeight)
{
    NX_DEBUG(this, "Render nvidia: %1x%2 texture(%3x%4), bitDepth: %5, m_impl->m_pitch: %6",
        m_impl->m_width, m_impl->m_height, textureWidth, textureHeight, bitDepth, m_impl->m_pitch);
    if (!m_impl->resize(frameData, m_impl->m_pitch, textureWidth, textureHeight, m_impl->m_width, m_impl->m_height, bitDepth))
        return false;

    if (!m_impl->convertToRgb((uint8_t*)m_impl->m_resizedFrame, textureWidth, textureHeight, format, bitDepth, matrix))
        return false;

    return m_impl->render(textureId, textureWidth, textureHeight);
}

} // namespace nx::media::nvidia::linux

#endif // __linux__