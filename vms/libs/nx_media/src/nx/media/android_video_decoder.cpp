// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "android_video_decoder.h"

#if defined(Q_OS_ANDROID)

#include <QtCore/QCache>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QJniEnvironment>
#include <QtCore/QJniObject>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/private/qopenglextensions_p.h>
#include <QtMultimedia/QVideoFrameFormat>
#include <QtMultimedia/private/qhwvideobuffer_p.h>
#include <QtOpenGL/QOpenGLFramebufferObject>
#include <QtOpenGL/QOpenGLShaderProgram>

#include <nx/media/annexb_to_mp4.h>
#include <nx/media/avframe_memory_buffer.h>
#include <nx/media/ffmpeg/hw_video_decoder.h>
#include <nx/media/h264_utils.h>
#include <nx/media/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/thread/mutex.h>

extern "C" {
#include <libavcodec/jni.h>
#include <libavcodec/mediacodec.h>
} // extern "C"

#include "fbo_manager.h"

namespace nx::media {

namespace {

static const GLfloat g_vertex_data[] = {
    -1.f, 1.f,
    1.f, 1.f,
    1.f, -1.f,
    -1.f, -1.f
};

static const GLfloat g_texture_data[] = {
    0.f, 0.f,
    1.f, 0.f,
    1.f, 1.f,
    0.f, 1.f
};

/**
 * Convert codec from ffmpeg enum to Android codec string representation.
 * Only codecs listed below are supported by AndroidVideoDecoder.
 */
static QString androidMimeForCodec(AVCodecID codecId)
{
    switch(codecId)
    {
        case AV_CODEC_ID_H265:
            return "video/hevc";
        case AV_CODEC_ID_H264:
            return "video/avc";
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
            return "video/3gpp";
        case AV_CODEC_ID_MPEG4:
            return "video/mp4v-es";
        case AV_CODEC_ID_MPEG2VIDEO:
            return "video/mpeg2";
        case AV_CODEC_ID_VP8:
            return "video/x-vnd.on2.vp8";
        case AV_CODEC_ID_VP9:
            return "video/x-vnd.on2.vp9";
        case AV_CODEC_ID_AV1:
            return "video/av01";
        default:
            return QString();
    }
}

#define CHECK_GL_ERROR \
    if (const auto error = funcs->glGetError()) \
        qDebug() << QString("gl error %1").arg(error); \

} // namespace

//-------------------------------------------------------------------------------------------------

class TextureBuffer: public QHwVideoBuffer
{
public:
    TextureBuffer(FboTextureHolder textureHolder, const QVideoFrameFormat& format)
        :
        QHwVideoBuffer(QVideoFrame::RhiTextureHandle),
        m_textureHolder(std::move(textureHolder)),
        m_format(format)
    {
    }

    ~TextureBuffer()
    {
    }

    virtual MapData map(QVideoFrame::MapMode) override
    {
        return {};
    }

    virtual void unmap() override
    {
    }

    virtual quint64 textureHandle(QRhi&, int plane) override
    {
        return plane == 0 ? m_textureHolder.textureId() : 0;
    }

    QVideoFrameFormat format() const
    {
        return m_format;
    }

private:
    FboTextureHolder m_textureHolder;
    QVideoFrameFormat m_format;
};

//-------------------------------------------------------------------------------------------------
// AndroidVideoDecoder::Private

class AndroidVideoDecoder::Private
{
public:
    Private(const RenderContextSynchronizerPtr& synchronizer):
        outFramePtr(new CLVideoDecoderOutput()),
        initialized(false),
        synchronizer(synchronizer),
        program(nullptr)
    {
        static std::once_flag initFfmpegJvmFlag;
        std::call_once(initFfmpegJvmFlag,
            []()
            {
                av_jni_set_java_vm(QJniEnvironment::javaVM(), nullptr);
            });
    }

    ~Private()
    {
        if (decoder)
            closeCodecContext();
    }

    void updateTexImage()
    {
        surfaceTexture.callMethod<void>("updateTexImage");
    }

    QMatrix4x4 getTransformMatrix()
    {
        QMatrix4x4 matrix;

        QJniEnvironment env;
        jfloatArray array = env->NewFloatArray(16);
        surfaceTexture.callMethod<void>("getTransformMatrix", "([F)V", array);
        env->GetFloatArrayRegion(array, 0, 16, matrix.data());
        env->DeleteLocalRef(array);

        return matrix;
    }

    FboTextureHolder renderFrameToFbo();
    void createGlResources();

    static QSize getMaxResolution(const AVCodecID codec);
    static QJniObject getVideoCapabilities(const AVCodecID codec);
    static bool isSizeSupported(const AVCodecID codec, const QSize& size);

    void initContext(const QnConstCompressedVideoDataPtr& frame)
    {
        if (!frame || !frame->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
            return;

        if (!nx::media::ffmpeg::HwVideoDecoder::isCompatible(
            frame->compressionType,
            QSize(frame->width, frame->height),
            /* allowOverlay */ false))
        {
            NX_WARNING(this, "Decoder is not compatible with codec %1 resolution %2x%3",
                frame->compressionType, frame->width, frame->height);
            return;
        }

        NX_INFO(this, "Initializing decoder for codec %1 resolution %2x%3...",
            frame->compressionType, frame->width, frame->height);

        if (textureId == 0)
        {
            glGenTextures(1, &textureId);
            if (textureId == 0)
            {
                NX_WARNING(this, "Failed to allocate GL texture");
                return;
            }
        }

        auto deleteGlTextureId = nx::utils::ScopeGuard(
            [this]()
            {
                if (textureId != 0)
                {
                    glDeleteTextures(1, &textureId);
                    textureId = 0;
                }
            });

        QJniEnvironment env;

        surfaceTexture = QJniObject(
            "android/graphics/SurfaceTexture", (jint) textureId, (jboolean) false);

        if (!surfaceTexture.isValid())
        {
            NX_WARNING(this, "Failed to initialize surface texture");
            return;
        }

        surface = QJniObject(
            "android/view/Surface",
            "(Landroid/graphics/SurfaceTexture;)V",
            surfaceTexture.object());

        if (!surface.isValid())
        {
            NX_WARNING(this, "Failed to initialize surface");
            surfaceTexture = {};
            return;
        }

        deleteGlTextureId.disarm();

        decoder = std::make_unique<nx::media::ffmpeg::HwVideoDecoder>(
            AV_HWDEVICE_TYPE_MEDIACODEC,
            /* metrics */ nullptr,
            /* device */ std::string{},
            /* options */ nullptr,
            [this](AVHWDeviceContext* deviceContext)
            {
                if (deviceContext->type != AV_HWDEVICE_TYPE_MEDIACODEC)
                {
                    NX_WARNING(this, "Unexpected device context type: %1", deviceContext->type);
                    return false;
                }

                auto hwctx = (AVMediaCodecContext*) deviceContext->hwctx;
                if (!hwctx)
                {
                    NX_WARNING(this, "MediaCodec context not allocated");
                    return false;
                }

                hwctx->surface = surface.object<jobject>();
                NX_DEBUG(this, "MediaCodec context with surface %1", hwctx->surface);
                return true;
            });
    }

    void closeCodecContext()
    {
        frameSize = {};
        decoder.reset();
        surface = {};

        if (surfaceTexture.isValid())
            surfaceTexture.callMethod<void>("release");
        surfaceTexture = {};

        if (textureId != 0)
        {
            glDeleteTextures(1, &textureId);
            textureId = 0;
        }
    }

    static QMap<AVCodecID, QSize> maxResolutions;
    static QMutex maxResolutionsMutex;

    std::unique_ptr<nx::media::ffmpeg::HwVideoDecoder> decoder;
    AnnexbToMp4 m_annexbToMp4;
    CLVideoDecoderOutputPtr outFramePtr;

    bool initialized = false;
    QJniObject surface;
    QJniObject surfaceTexture;
    RenderContextSynchronizerPtr synchronizer;
    QSize frameSize;

    std::unique_ptr<QOpenGLShaderProgram> program;
    std::unique_ptr<QOpenGLContext> threadGlCtx;
    std::unique_ptr<QOffscreenSurface> offscreenSurface;

    GLuint textureId = 0;
    FboManager fboManager;
};

QMap<AVCodecID, QSize> AndroidVideoDecoder::Private::maxResolutions;
QMutex AndroidVideoDecoder::Private::maxResolutionsMutex;

FboTextureHolder AndroidVideoDecoder::Private::renderFrameToFbo()
{
    QOpenGLFunctions* funcs = QOpenGLContext::currentContext()->functions();

    createGlResources();

    return fboManager.getTexture(
        [this, funcs](FboHolder& fbo)
        {
            updateTexImage();

            // Save current state of rendering context.
            GLboolean stencilTestEnabled;
            GLboolean depthTestEnabled;
            GLboolean scissorTestEnabled;
            GLboolean blendEnabled;
            glGetBooleanv(GL_STENCIL_TEST, &stencilTestEnabled);
            glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
            glGetBooleanv(GL_SCISSOR_TEST, &scissorTestEnabled);
            glGetBooleanv(GL_BLEND, &blendEnabled);
            #if 0 //< No need to save/restore FBO binding because this context is not used anywhere else.
                GLuint prevFbo = 0;
                funcs->glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*) &prevFbo);
            #endif // 0

            CHECK_GL_ERROR

            if (stencilTestEnabled)
                glDisable(GL_STENCIL_TEST);
            if (depthTestEnabled)
                glDisable(GL_DEPTH_TEST);
            if (scissorTestEnabled)
                glDisable(GL_SCISSOR_TEST);
            if (blendEnabled)
                glDisable(GL_BLEND);

            fbo.bind();

            glViewport(0, 0, frameSize.width(), frameSize.height());
            CHECK_GL_ERROR

            program->bind();
            CHECK_GL_ERROR
            program->enableAttributeArray(0);
            CHECK_GL_ERROR
            program->enableAttributeArray(1);
            CHECK_GL_ERROR
            program->setUniformValue("texMatrix", getTransformMatrix());
            CHECK_GL_ERROR

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, g_vertex_data);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, g_texture_data);

            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            program->disableAttributeArray(0);
            program->disableAttributeArray(1);

            glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
            CHECK_GL_ERROR

            fbo.release();

            // Restore state of rendering context.
            #if 0 //< No need to save/restore FBO binding because this context is not used anywhere else.
                funcs->glBindFramebuffer(GL_FRAMEBUFFER, prevFbo); // sets both READ and DRAW
            #endif // 0

            if (stencilTestEnabled)
                glEnable(GL_STENCIL_TEST);
            if (depthTestEnabled)
                glEnable(GL_DEPTH_TEST);
            if (scissorTestEnabled)
                glEnable(GL_SCISSOR_TEST);
            if (blendEnabled)
                glEnable(GL_BLEND);

            glFlush();
            glFinish();

            static_cast<QOpenGLExtensions *>(funcs)->flushShared();
        });
}

void AndroidVideoDecoder::Private::createGlResources()
{
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    QOpenGLFunctions* funcs = ctx->functions();

    if (!program)
    {
        program = std::make_unique<QOpenGLShaderProgram>();

        QOpenGLShader* vertexShader = new QOpenGLShader(QOpenGLShader::Vertex, program.get());
        vertexShader->compileSourceCode(
            "attribute highp vec4 vertexCoordsArray; \n"
            "attribute highp vec2 textureCoordArray; \n"
            "uniform   highp mat4 texMatrix; \n"
            "varying   highp vec2 textureCoords; \n"
            "void main(void) \n"
            "{ \n"
            "    gl_Position = vertexCoordsArray; \n"
            "    textureCoords = (texMatrix * vec4(textureCoordArray, 0.0, 1.0)).xy; \n"
            "}\n");
        program->addShader(vertexShader);
        CHECK_GL_ERROR

        QOpenGLShader* fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, program.get());
        fragmentShader->compileSourceCode(
            "#extension GL_OES_EGL_image_external : require \n"
            "varying highp vec2         textureCoords; \n"
            "uniform samplerExternalOES frameTexture; \n"
            "void main() \n"
            "{ \n"
            "    gl_FragColor = texture2D(frameTexture, textureCoords); \n"
            "}\n");
        program->addShader(fragmentShader);
        CHECK_GL_ERROR

        program->bindAttributeLocation("vertexCoordsArray", 0);
        program->bindAttributeLocation("textureCoordArray", 1);
        program->link();
        CHECK_GL_ERROR
    }
}

//-------------------------------------------------------------------------------------------------
// AndroidVideoDecoder

AndroidVideoDecoder::AndroidVideoDecoder(
    const RenderContextSynchronizerPtr& synchronizer,
    const QSize& /*resolution*/)
    :
    AbstractVideoDecoder(),
    d(new Private(synchronizer))
{
    auto sharedContext = QOpenGLContext::globalShareContext();
    if (!NX_ASSERT(sharedContext))
        return;

    d->threadGlCtx.reset(new QOpenGLContext());
    d->threadGlCtx->setShareContext(sharedContext);
    d->threadGlCtx->setFormat(sharedContext->format());

    if (d->threadGlCtx->create() && d->threadGlCtx->shareContext())
    {
        NX_DEBUG(this, "Using shared OpenGL context");
        d->offscreenSurface.reset(new QOffscreenSurface());
        d->offscreenSurface->setFormat(d->threadGlCtx->format());
        d->offscreenSurface->create();

        d->threadGlCtx->makeCurrent(d->offscreenSurface.get());
    }
    else
    {
        d->threadGlCtx.reset();
    }
}

AndroidVideoDecoder::~AndroidVideoDecoder()
{
}

QJniObject AndroidVideoDecoder::Private::getVideoCapabilities(const AVCodecID codec)
{
    const QString codecMimeType = androidMimeForCodec(codec);
    if (codecMimeType.isEmpty())
        return {};

    QJniEnvironment env;
    const jclass codecListClass = env.findClass("android/media/MediaCodecList");
    const jmethodID getCodecInfoAtMethodId = env.findStaticMethod(
        codecListClass, "getCodecInfoAt", "(I)Landroid/media/MediaCodecInfo;");

    const auto numCodecs = QJniObject::callStaticMethod<jint>(codecListClass, "getCodecCount");

    for (jint i = 0; i < numCodecs; ++i)
    {
        const QJniObject codecInfo = QJniObject::callStaticMethod<jobject>(
            codecListClass, getCodecInfoAtMethodId, i);

        if (!codecInfo.isValid() || codecInfo.callMethod<jboolean>("isEncoder"))
            continue;

        const auto supportedMimeTypes = codecInfo.callMethod<QJniArray<jstring>>(
            "getSupportedTypes", "()[Ljava/lang/String;").toContainer<QStringList>();

        if (!supportedMimeTypes.contains(codecMimeType, Qt::CaseInsensitive))
            continue;

        const auto caps = codecInfo.callMethod<jobject>(
            "getCapabilitiesForType",
            "(Ljava/lang/String;)Landroid/media/MediaCodecInfo$CodecCapabilities;",
            QJniObject::fromString(codecMimeType));

        if (!caps.isValid())
            continue;

        const auto vcaps = caps.callMethod<jobject>(
            "getVideoCapabilities", "()Landroid/media/MediaCodecInfo$VideoCapabilities;");

        if (vcaps.isValid())
            return vcaps;
    }

    return {};
}

QSize AndroidVideoDecoder::Private::getMaxResolution(const AVCodecID codec)
{
    QMutexLocker lock(&maxResolutionsMutex);

    auto existingMaxResolutionIterator = maxResolutions.find(codec);
    if (existingMaxResolutionIterator != maxResolutions.end())
        return existingMaxResolutionIterator.value();

    auto vcaps = getVideoCapabilities(codec);

    if (!vcaps.isValid())
        return {};

    const jint h = vcaps.callMethod<jobject>("getSupportedHeights", "()Landroid/util/Range;")
        .callMethod<jobject>("getUpper", "()Ljava/lang/Comparable;")
        .callMethod<jint>("intValue");

    const jint w = h > 0
        ? vcaps.callMethod<jobject>("getSupportedWidthsFor",
                "(I)Landroid/util/Range;", h)
            .callMethod<jobject>("getUpper", "()Ljava/lang/Comparable;")
            .callMethod<jint>("intValue")
        : 0;

    QSize maxSize{w, h};

    if (maxSize.isEmpty())
    {
        NX_WARNING(NX_SCOPE_TAG,
            nx::format("Android Video Decoder failed to report max resolution for codec %1")
            .arg(avcodec_get_name(codec)));
    }
    else
    {
        NX_INFO(NX_SCOPE_TAG,
            nx::format("Maximum hardware decoder resolution: %1 for codec %2")
            .arg(maxSize).arg(avcodec_get_name(codec)));
        maxResolutions[codec] = maxSize;
    }

    return maxSize;
}

bool AndroidVideoDecoder::Private::isSizeSupported(const AVCodecID codec, const QSize& size)
{
    const auto vcaps = AndroidVideoDecoder::Private::getVideoCapabilities(codec);
    if (!vcaps.isValid())
        return false;

    return vcaps.callMethod<jboolean>(
        "isSizeSupported", "(II)Z", (jint) size.width(), (jint) size.height());
}

bool AndroidVideoDecoder::isCompatible(
    const AVCodecID codec,
    const QSize& resolution,
    bool allowOverlay,
    bool allowHardwareAcceleration)
{
    if (!allowHardwareAcceleration)
        return false;

    if (!allowOverlay) //< Considering AndroidVideoDecoder a hardware one.
        return false;

    if (!AndroidVideoDecoder::Private::isSizeSupported(codec, resolution))
    {
        NX_WARNING(NX_SCOPE_TAG,
            nx::format("Codec for %1 is not compatible with resolution %2")
                .arg(androidMimeForCodec(codec))
                .arg(resolution));
        return false;
    }

    NX_DEBUG(NX_SCOPE_TAG,
        nx::format("Codec %1 is compatible with resolution %2")
            .arg(androidMimeForCodec(codec))
            .arg(resolution));

    return true;
}

QSize AndroidVideoDecoder::maxResolution(const AVCodecID codec)
{
    return AndroidVideoDecoder::Private::getMaxResolution(codec);
}

bool AndroidVideoDecoder::sendPacket(const QnConstCompressedVideoDataPtr& packet)
{
    auto compressedVideoData = packet;
    if (packet && nx::media::isAnnexb(packet.get()))
    {
        compressedVideoData = d->m_annexbToMp4.process(packet.get());
        if (!compressedVideoData)
        {
            NX_WARNING(this, "Decoding failed, cannot convert to MP4 format");
            return false;
        }
    }

    if (d->frameSize.isEmpty())
    {
        if (compressedVideoData)
            d->frameSize = getFrameSize(compressedVideoData.get());
        if (d->frameSize.isEmpty())
        {
            NX_DEBUG(this, "Failed to initialize framebuffer, waiting for key frame");
            return false; //< Wait for I frame to be able to extract data from the binary stream.
        }
        d->fboManager.init(d->frameSize);
    }

    if (!d->decoder)
    {
        d->initContext(compressedVideoData);
        if (!d->decoder)
            return false;
    }

    return d->decoder->sendPacket(compressedVideoData);
}

bool AndroidVideoDecoder::receiveFrame(VideoFramePtr* decodedFrame)
{
    if (!d->decoder)
    {
        NX_WARNING(this, "Decoder is not initialized");
        return false;
    }

    if (!d->decoder->receiveFrame(&d->outFramePtr) || !d->outFramePtr)
    {
        const bool flushed = d->decoder->getLastDecodeResult() == 0;
        if (flushed)
            decodedFrame->reset();
        return flushed;
    }

    const qint64 startTimeMs = d->outFramePtr->pts == DATETIME_INVALID
        ? DATETIME_INVALID
        : d->outFramePtr->pts / 1000; //< Convert usec to msec.

    auto recreateFrameGuard = nx::utils::makeScopeGuard(
        [this]()
        {
            d->outFramePtr.reset(new CLVideoDecoderOutput());
        });

    const auto pixelFormat = static_cast<AVPixelFormat>(d->outFramePtr->format);

    if (pixelFormat != AV_PIX_FMT_MEDIACODEC
        && AvFrameMemoryBuffer::toQtPixelFormat(pixelFormat) != QVideoFrameFormat::Format_Invalid)
    {
        // Software frame format.

        auto videoFrame = new VideoFrame(
            std::make_unique<CLVideoDecoderOutputMemBuffer>(d->outFramePtr));
        videoFrame->setStartTime(startTimeMs);
        decodedFrame->reset(videoFrame);
        return true;
    }

    if (d->outFramePtr->data[3])
    {
        if (av_mediacodec_release_buffer(
            (AVMediaCodecBuffer*) d->outFramePtr->data[3], /*render*/ 1) < 0)
        {
            NX_WARNING(this, "Failed to render MediaCodec buffer");
            return false;
        }
    }
    else
    {
        NX_WARNING(this, "No MediaCodec buffer found in output frame");
        return false;
    }

    if (d->frameSize.isEmpty())
    {
        NX_DEBUG(this, "Failed to initialize framebuffer, waiting for key frame");
        return false;
    }

    FboTextureHolder textureHolder;

    if (d->textureId == 0)
    {
        NX_WARNING(this, "Failed to allocate GL texture");
        return false;
    }

    if (d->threadGlCtx)
    {
        textureHolder = d->renderFrameToFbo();
    }
    else
    {
        d->synchronizer->execInRenderContext(
            [&textureHolder, this](void*)
            {
                textureHolder = d->renderFrameToFbo();
            },
            nullptr);
    }

    if (textureHolder.isNull())
    {
        NX_WARNING(this, "Failed to render frame to FBO");
        return false;
    }

    auto videoFrame = new VideoFrame(std::make_unique<TextureBuffer>(
        std::move(textureHolder),
        QVideoFrameFormat(d->frameSize, QVideoFrameFormat::Format_BGRX8888)));
    videoFrame->setStartTime(startTimeMs);

    decodedFrame->reset(videoFrame);

    return true;
}

int AndroidVideoDecoder::currentFrameNumber() const
{
    if (!d->decoder)
        return -1;

    return d->decoder->currentFrameNumber();
}

AbstractVideoDecoder::Capabilities AndroidVideoDecoder::capabilities() const
{
    if (d->decoder && !d->decoder->hardwareDecoder())
        return Capability::noCapability;

    return Capability::hardwareAccelerated;
}

} // namespace nx::media

#endif // defined(Q_OS_ANDROID)
