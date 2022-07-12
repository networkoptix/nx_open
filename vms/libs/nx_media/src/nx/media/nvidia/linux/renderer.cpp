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
    bool convertToRgb(uint8_t* frameData, cudaVideoSurfaceFormat format, int bitDepth, int matrix);
    bool render(
        bool isNewTexture,
        GLuint textureId,
        uint8_t* frameData,
        cudaVideoSurfaceFormat format,
        int bitDepth,
        int matrix);

private:
    int m_width = 0;
    int m_height = 0;

    CUgraphicsResource m_cuResource;
    GLuint m_pbo; //< Buffer object to upload texture data
    std::unique_ptr<QOffscreenSurface> m_surface;
    std::unique_ptr<QOpenGLContext> m_context;
};

RendererImpl::~RendererImpl()
{
    cuGraphicsUnregisterResource(m_cuResource);

    // Release OpenGL resources.
    glDeleteBuffers(1, &m_pbo);

}

bool RendererImpl::initialize(int width, int height)
{
    m_surface = std::make_unique<QOffscreenSurface>();
    m_context = std::make_unique<QOpenGLContext>();
    m_context->setShareContext(QOpenGLContext::globalShareContext());
    m_context->setObjectName("NvidiaDecoderContext");
    NX_ASSERT(m_context->create());
    m_surface->create();
    m_context->makeCurrent(m_surface.get());

    m_width = width;
    m_height = height;

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        NX_WARNING(this, "glewInit failed: %1", glewGetErrorString(err));
        return false;
    }

    glGenBuffers(1, &m_pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, m_width * m_height * 4, NULL, GL_DYNAMIC_DRAW);
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

bool RendererImpl::convertToRgb(
    uint8_t* frameData, cudaVideoSurfaceFormat format, int bitDepth, int matrix)
{
    int pitch = m_width * 4;

    auto status = cuGraphicsMapResources(1, &m_cuResource, 0);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to map resource: %1", toString(status));
        return false;
    }

    size_t nSize = 0;
    CUdeviceptr deviceBuffer;
    status = cuGraphicsResourceGetMappedPointer(&deviceBuffer, &nSize, m_cuResource);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to map resource: %1", toString(status));
        return false;
    }

    // Launch cuda kernels for colorspace conversion from raw video to raw image formats which
    // OpenGL textures can work with
    if (bitDepth == 8)
    {
        if (format == cudaVideoSurfaceFormat_YUV444)
            YUV444ToColor32<BGRA32>(frameData, m_width, (uint8_t*)deviceBuffer, pitch, m_width, m_height, matrix);
        else // default assumed NV12
            Nv12ToColor32<BGRA32>(frameData, m_width, (uint8_t*)deviceBuffer, pitch, m_width, m_height, matrix);
    }
    else
    {
        if (format == cudaVideoSurfaceFormat_YUV444)
            YUV444P16ToColor32<BGRA32>(frameData, 2 * m_width, (uint8_t*)deviceBuffer, pitch, m_width, m_height, matrix);
        else // default assumed P016
            P016ToColor32<BGRA32>(frameData, 2 * m_width, (uint8_t*)deviceBuffer, pitch, m_width, m_height, matrix);
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


bool RendererImpl::render(
    bool isNewTexture,
    GLuint textureId,
    uint8_t* frameData,
    cudaVideoSurfaceFormat format,
    int bitDepth,
    int matrix)
{
    NX_INFO(this, "QQQ Render nvidia: %1x%2", m_width, m_height);
    if (isNewTexture)
    {
        NX_INFO(this, "QQQ New texture");
    }

    //if (!convertToRgb(frameData, (cudaVideoSurfaceFormat)format, bitDepth, matrix))
      //  return false;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
    auto error = glGetError();
    if (error != GL_NO_ERROR)
        NX_WARNING(NX_SCOPE_TAG, "OpenGL error in glBindBuffer: %1, pbo: %2", glErrorString(error), m_pbo);

    glBindTexture(GL_TEXTURE_2D, textureId);
    error = glGetError();
    if (error != GL_NO_ERROR)
        NX_WARNING(NX_SCOPE_TAG, "OpenGL error in glBindTexture: %1, textureId: %2", glErrorString(error), textureId);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_BGRA, GL_UNSIGNED_BYTE, 0);
    error = glGetError();
    if (error != GL_NO_ERROR)
        NX_WARNING(NX_SCOPE_TAG, "OpenGL error in glTexSubImage2D: %1", glErrorString(error));

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

bool Renderer::initialize(int width, int height)
{
    m_impl = std::make_unique<RendererImpl>();
    return m_impl->initialize(width, height);
}

bool Renderer::convertToRgb(
    uint8_t* frameData, cudaVideoSurfaceFormat format, int bitDepth, int matrix)
{
    return m_impl->convertToRgb(frameData, format, bitDepth, matrix);
}

bool Renderer::render(
        bool isNewTexture,
        GLuint textureId,
        uint8_t* frameData,
        cudaVideoSurfaceFormat format,
        int bitDepth,
        int matrix)
{
    return m_impl->render(isNewTexture, textureId, frameData, format, bitDepth, matrix);
}

} // namespace nx::media::nvidia::linux

#endif // __linux__