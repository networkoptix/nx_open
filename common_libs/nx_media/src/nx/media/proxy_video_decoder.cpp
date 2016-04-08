#include "proxy_video_decoder.h"

// Configuration
#define xENABLE_RGB //< Use ProxyDecoder::decodeToRgb() to AlignedMemVideoBuffer, without OpenGL.
#define ENABLE_YUV //< Use ProxyDecoder::decodeToYuvPlanar() to AlignedMemVideoBuffer, without OpenGL.
#define xENABLE_LOG
#define ENABLE_TIME

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLTexture>

#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>
#include <QtOpenGL/QtOpenGL>

#include <QtCore/QThread>

#include <EGL/egl.h>
#include <EGL/eglext.h>

// Interface for the external lib 'proxydecoder'.
#include <proxy_decoder.h>

//#include <utils/media/ffmpeg_helper.h>

#include "aligned_mem_video_buffer.h"
#include "abstract_resource_allocator.h"

// TODO mike: Remove when frame size is passed from SeamlessVideoDecoder.
#include <utils/media/jpeg_utils.h>
#include <utils/media/h264_utils.h>
#include <nx/utils/log/log.h>

#include "proxy_video_decoder_utils.cxx"

namespace nx {
namespace media {

#if defined(ENABLE_RGB)

    #include "proxy_video_decoder_rgb.cxx"

#elif defined(ENABLE_YUV)

    #include "proxy_video_decoder_yuv.cxx"

#else // ENABLE_RGB || ENABLE_YUV

    #include "proxy_video_decoder_gl.cxx"

#endif // ENABLE_RGB || ENABLE_YUV

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
    Q_UNUSED(resolution);
    return codec == CODEC_ID_H264;
}

int ProxyVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* decodedFrame)
{
    decodedFrame->reset();

    if (!compressedVideoData && !d->isInitialized())
        return 0;

    if (!d->isInitialized())
        d->initialize(compressedVideoData);

    return d->decode(compressedVideoData, decodedFrame);
}

void ProxyVideoDecoder::setAllocator(AbstractResourceAllocator* allocator)
{
    d->setAllocator(allocator);
}

} // namespace media
} // namespace nx
