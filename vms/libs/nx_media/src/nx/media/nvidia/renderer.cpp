// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/utils/log/log.h>

#include "renderer.h"

#include <cuda.h>
#include <cudaGL.h>

#include <nx/media/nvidia/nvidia_utils.h>
#include <Utils/NvCodecUtils.h>
#include <Utils/ColorSpace.h>
#include <nx/media/nvidia/nvidia_driver_proxy.h>
#include <nx/media/nvidia/nvidia_video_frame.h>

static QString glErrorString(GLenum err)
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

namespace nx::media::nvidia {

Renderer::~Renderer()
{
    freePbo();
    freeScaledFrame();
}

void Renderer::freeScaledFrame()
{
    if (m_scaledFrame)
    {
        NvidiaDriverApiProxy::instance().cuMemFree(m_scaledFrame);
        m_scaledFrame = 0;
    }
}

void Renderer::freePbo()
{
    if (m_cuResource)
    {
        NvidiaDriverApiProxy::instance().cuGraphicsUnregisterResource(m_cuResource);
        m_cuResource = nullptr;
    }

    if (m_pbo)
    {
        glDeleteBuffers(1, &m_pbo);
        m_pbo = 0;
    }
}

bool Renderer::initializeScaledFrame(int width, int height)
{
    freeScaledFrame();

    int size = width * (height + height / 2);
    auto status = NvidiaDriverApiProxy::instance().cuMemAlloc(&m_scaledFrame, size);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to alloc buffer: %1", toString(status));
        return false;
    }
    status = NvidiaDriverApiProxy::instance().cuMemsetD8(m_scaledFrame, 0, size);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to memset buffer: %1", toString(status));
        return false;
    }
    return true;
}

bool Renderer::initializePbo(int width, int height)
{
    NX_DEBUG(this, "Render init: %1x%2", width, height);
    freePbo();

    QOpenGLFunctions::initializeOpenGLFunctions();

    glGenBuffers(1, &m_pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * 4, NULL, GL_DYNAMIC_DRAW);
    auto glError = glGetError();
    if (GL_NO_ERROR != glError)
    {
        m_pbo = 0;
        NX_WARNING(this, "Failed to create GL buffer: %1", glError);
        return false;
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // Attach pbo to the cuda graphics resource
    auto status = NvidiaDriverApiProxy::instance().cuGraphicsGLRegisterBuffer(
        &m_cuResource, m_pbo, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to attach pbo: %1", toString(status));
        return false;
    }

    return true;
}

bool Renderer::resize(const NvidiaVideoFrame& frame, const QSize& dstSize)
{
    if (frame.bitDepth == 8)
        ResizeNv12((uint8_t*)m_scaledFrame, dstSize.width(), dstSize.width(), dstSize.height(), frame.frameData, frame.pitch, frame.width, frame.height);
    else
        ResizeP016((uint8_t*)m_scaledFrame, dstSize.width(), dstSize.width(), dstSize.height(), frame.frameData, frame.pitch, frame.width, frame.height);
    return true;
}

bool Renderer::convertToRgb(
    uint8_t* frameData, const QSize& size, int pitch, int format, int bitDepth, int matrix)
{
    int width = size.width();
    int height = size.height();
    int dstPitch = size.width() * 4;
    auto status = NvidiaDriverApiProxy::instance().cuGraphicsMapResources(1, &m_cuResource, 0);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to map resource: %1", toString(status));
        return false;
    }

    size_t bufferSize = 0;
    CUdeviceptr deviceBuffer;
    status = NvidiaDriverApiProxy::instance().cuGraphicsResourceGetMappedPointer(&deviceBuffer, &bufferSize, m_cuResource);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to get mapped pointer: %1", toString(status));
        return false;
    }

    // Launch cuda kernels for colorspace conversion from raw video to raw image formats which
    // OpenGL textures can work with.
    if (bitDepth == 8)
    {
        if (format == cudaVideoSurfaceFormat_YUV444)
            YUV444ToColor32<BGRA32>(frameData, pitch, (uint8_t*)deviceBuffer, dstPitch, width, height, matrix);
        else // default assumed NV12
            Nv12ToColor32<BGRA32>(frameData, pitch, (uint8_t*)deviceBuffer, dstPitch, width, height, matrix);
    }
    else
    {
        if (format == cudaVideoSurfaceFormat_YUV444)
            YUV444P16ToColor32<BGRA32>(frameData, 2 * pitch, (uint8_t*)deviceBuffer, dstPitch, width, height, matrix);
        else // default assumed P016
            P016ToColor32<BGRA32>(frameData, 2 * pitch, (uint8_t*)deviceBuffer, dstPitch, width, height, matrix);
    }

    status = NvidiaDriverApiProxy::instance().cuGraphicsUnmapResources(1, &m_cuResource, 0);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to unmap resource: %1", toString(status));
        return false;
    }
    return true;
}

bool Renderer::draw(GLuint textureId, int width, int height)
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

bool Renderer::render(GLuint textureId, QSize textureSize, const NvidiaVideoFrame& frame)
{
    QSize frameSize = QSize(frame.width, frame.height);
    NX_VERBOSE(this, "Render nvidia frame: %1 texture: %2, bitDepth: %3, pitch: %4",
        frameSize, textureSize, frame.bitDepth, frame.pitch);

    if (textureSize != m_textureSize)
    {
        if (!initializePbo(textureSize.width(), textureSize.height()))
            return false;

        if (!initializeScaledFrame(textureSize.width(), textureSize.height()))
             return false;

        m_textureSize = textureSize;
    }

    uint8_t* data = frame.frameData;
    int pitch = frame.pitch;
    if (frameSize != textureSize)
    {
        if (!resize(frame, textureSize))
            return false;
        data = (uint8_t*)m_scaledFrame;
        pitch = textureSize.width();
    }

    if (!convertToRgb(data, textureSize, pitch, (cudaVideoSurfaceFormat)frame.format, frame.bitDepth, frame.matrix))
        return false;

    return draw(textureId, textureSize.width(), textureSize.height());
}

} // namespace nx::media::nvidia
