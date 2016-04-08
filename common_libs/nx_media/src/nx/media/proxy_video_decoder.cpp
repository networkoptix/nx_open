#include "proxy_video_decoder.h"

// Configuration
#define xENABLE_RGB //< Use ProxyDecoder::decodeToRgb(), without OpenGL.
#define xENABLE_LOG
#define ENABLE_TIME
#define xENABLE_GL_LOG
#define ENABLE_GL_FATAL_ERRORS
static const bool USE_GUI_RENDERING = false;
static const bool USE_SHARED_CTX = true;

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

#ifdef ENABLE_RGB

#include "proxy_video_decoder_rgb.cxx"

#else // ENABLE_RGB

#include "proxy_video_decoder_gl_utils.cxx"
#include "proxy_video_decoder_gl.cxx"

#endif // ENABLE_RGB

//-------------------------------------------------------------------------------------------------
// ProxyVideoDecoder

ProxyVideoDecoder::ProxyVideoDecoder()
:
    d(new ProxyVideoDecoderPrivate(this))
{
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
