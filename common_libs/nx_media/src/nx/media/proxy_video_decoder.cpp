#include "proxy_video_decoder.h"

// Configuration
#define xENABLE_GL //< Use ProxyDecoder::decodeYuvPlanar() and then convert to RGB via OpenGL.
#define xENABLE_RGB //< Use ProxyDecoder::decodeToRgb() to AlignedMemVideoBuffer, without OpenGL.
#define xENABLE_YUV //< Use ProxyDecoder::decodeToYuvPlanar() to AlignedMemVideoBuffer, without OpenGL.
#define ENABLE_DISPLAY //< Use ProxyDecoder::decodeToDisplay().
#define ENABLE_LOG
#define ENABLE_TIME
static const bool ENABLE_LARGE_ONLY = true; //< isCompatible() will allow only width > 640.

// Interface for the external lib 'proxydecoder'.
#include <proxy_decoder.h>

#include <cstdlib>

#include "aligned_mem_video_buffer.h"
#include "abstract_resource_allocator.h"

// TODO mike: Remove when frame size is passed from SeamlessVideoDecoder.
#include <utils/media/jpeg_utils.h>
#include <utils/media/h264_utils.h>
#include <nx/utils/log/log.h>

#include "proxy_video_decoder_utils.cxx"

// Should be included outside of namespaces.
#include <QtCore/QThread>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLFramebufferObject>                       
#include <QtGui/QOpenGLTexture>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QtOpenGL>
#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace nx {
namespace media {

#if defined(ENABLE_RGB) && !defined(ENABLE_YUV) && !defined(ENABLE_DISPLAY) && !defined(ENABLE_GL)
    #include "proxy_video_decoder_rgb.cxx"
#elif defined(ENABLE_YUV) && !defined(ENABLE_RGB) && !defined(ENABLE_DISPLAY) && !defined(ENABLE_GL)
    #include "proxy_video_decoder_yuv.cxx"
#elif defined(ENABLE_GL) && !defined(ENABLE_YUV) && !defined(ENABLE_RGB) && !defined(ENEABLE_DISPLAY)
    #include "proxy_video_decoder_gl.cxx"
#elif defined(ENABLE_DISPLAY) && !defined(ENABLE_YUV) && !defined(ENABLE_RGB) && !defined(ENEABLE_GL)
    #include "proxy_video_decoder_display.cxx"
#else
    #error "More than one of ENABLE_xxx selectors specified."
#endif // ENABLE_RGB || ENABLE_YUV || ENABLE_GL || ENABLE_DISPLAY

//-------------------------------------------------------------------------------------------------
// ProxyVideoDecoder

ProxyVideoDecoder::ProxyVideoDecoder()
:
    d(new ProxyVideoDecoderPrivate(this))
{
    static_assert(QN_BYTE_ARRAY_PADDING >= ProxyDecoder::CompressedFrame::kPaddingSize,
        "ProxyVideoDecoder: Insufficient padding size");
}

ProxyVideoDecoder::~ProxyVideoDecoder()
{
}

bool ProxyVideoDecoder::isCompatible(const CodecID codec, const QSize& resolution)
{
    if (codec != CODEC_ID_H264)
    {
        QLOG("ProxyVideoDecoder::isCompatible(codec:" << codec << ", resolution" << resolution
            << ") -> false: codec != CODEC_ID_H264");
        return false;
    }

    if (ENABLE_LARGE_ONLY && resolution.width() <= 640)
    {
        qWarning() << "ProxyVideoDecoder::isCompatible(codec:" << codec << ", resolution" << resolution
            << ") -> false: ENABLE_LARGE_ONLY";
        return false;
    }

    QLOG("ProxyVideoDecoder::isCompatible(codec:" << codec << ", resolution" << resolution 
        << ") -> true");
    return true;
}

int ProxyVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* decodedFrame)
{
    decodedFrame->reset();

    if (!compressedVideoData && !d->isInitialized())
        return 0;

    if (!d->isInitialized())
    {
        NX_CRITICAL(compressedVideoData);
        NX_CRITICAL(compressedVideoData->data());
        NX_CRITICAL(compressedVideoData->dataSize() > 0);

        // TODO mike: Pass frame size from SeamlessVideoDecoder.
        QSize frameSize;
        extractSpsPps(compressedVideoData, &frameSize, nullptr);

        d->initialize(frameSize);
    }

    return d->decode(compressedVideoData, decodedFrame);
}

void ProxyVideoDecoder::setAllocator(AbstractResourceAllocator* allocator)
{
    d->setAllocator(allocator);
}

} // namespace media
} // namespace nx
