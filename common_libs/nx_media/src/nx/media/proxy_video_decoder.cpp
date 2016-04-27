#include "proxy_video_decoder.h"
#if defined(ENABLE_PROXY_DECODER)

// Configuration
#define xENABLE_DISPLAY //< decodeToDisplayQueue() => displayDecoded() in video frame handle().
#define xENABLE_RGB //< decodeToRgb() -> AlignedMemVideoBuffer, without OpenGL.
#define xENABLE_YUV_PLANAR //< decodeToYuvPlanar() -> AlignedMemVideoBuffer, without OpenGL.
#define xENABLE_YUV_NATIVE //< decodeToYuvNative() -> AlignedMemVideoBuffer => Qt Plugin Shader.
#define ENABLE_GL //< decodeYuvPlanar() => Planar YUV Shader.

#define ENABLE_LOG
#define ENABLE_TIME
static const bool ENABLE_LARGE_ONLY = true; //< isCompatible() will allow only width > 640.

// Interface for the external lib 'proxydecoder'.
#include <proxy_decoder.h>

#include <cstdlib>

#include "aligned_mem_video_buffer.h"
#include "abstract_resource_allocator.h"

#include <nx/utils/log/log.h>

#include "proxy_video_decoder_utils.h"

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

#include "proxy_video_decoder_private.h"

namespace nx {
namespace media {

ProxyVideoDecoder::ProxyVideoDecoder(
    const ResourceAllocatorPtr& allocator, const QSize& resolution)
:
// TODO mike: Use conf.h.
#if defined(ENABLE_DISPLAY)
    d(ProxyVideoDecoderPrivate::createImplDisplay(this, allocator, resolution))
#elif defined(ENABLE_RGB)
    d(ProxyVideoDecoderPrivate::createImplRgb(this, allocator, resolution))
#elif defined(ENABLE_YUV_PLANAR)
    d(ProxyVideoDecoderPrivate::createImplYuvPlanar(this, allocator, resolution))
#elif defined(ENABLE_YUV_NATIVE)
    d(ProxyVideoDecoderPrivate::createImplYuvNative(this, allocator, resolution))
#elif defined(ENABLE_GL)
    d(ProxyVideoDecoderPrivate::createImplGl(this, allocator, resolution))
#endif
{
    static_assert(QN_BYTE_ARRAY_PADDING >= ProxyDecoder::CompressedFrame::kPaddingSize,
        "ProxyVideoDecoder: Insufficient padding size");

    // TODO: Consider moving this check to isCompatible().
    // Odd frame dimensions are not tested and can be unsupported due to UV having half-res.
    NX_CRITICAL(resolution.width() % 2 == 0 || resolution.height() % 2 == 0);
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
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame)
{
    return d->decode(compressedVideoData, outDecodedFrame);
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
