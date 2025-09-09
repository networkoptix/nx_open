// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QCoreApplication>
#include <QtCore/QJniEnvironment>
#include <QtCore/QJniObject>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/private/qopenglextensions_p.h>
#include <QtGui/rhi/qrhi.h>
#include <QtMultimedia/QVideoFrameFormat>
#include <QtMultimedia/private/qhwvideobuffer_p.h>
#include <QtOpenGL/QOpenGLFramebufferObject>
#include <QtOpenGL/QOpenGLShaderProgram>

#include <nx/media/annexb_to_mp4.h>
#include <nx/media/avframe_memory_buffer.h>
#include <nx/media/fbo_manager.h>
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

#include "hw_video_api.h"

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

#define CHECK_GL_ERROR \
    if (const auto error = funcs->glGetError()) \
        qDebug() << QString("gl error %1").arg(error);

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

class DecoderData: public VideoApiDecoderData
{
    using base_type = VideoApiDecoderData;

public:
    DecoderData(QRhi* rhi):
        base_type(rhi),
        initialized(false),
        program(nullptr)
    {
        static std::once_flag initFfmpegJvmFlag;
        std::call_once(initFfmpegJvmFlag,
            []()
            {
                av_jni_set_java_vm(QJniEnvironment::javaVM(), nullptr);
            });

        auto sharedContext = QOpenGLContext::globalShareContext();
        if (!NX_ASSERT(sharedContext))
            return;

        threadGlCtx.reset(new QOpenGLContext());
        threadGlCtx->setShareContext(sharedContext);
        threadGlCtx->setFormat(sharedContext->format());

        if (threadGlCtx->create() && threadGlCtx->shareContext())
        {
            NX_DEBUG(this, "Using shared OpenGL context");
            offscreenSurface.reset(new QOffscreenSurface());
            offscreenSurface->setFormat(threadGlCtx->format());
            offscreenSurface->create();

            threadGlCtx->makeCurrent(offscreenSurface.get());
        }
        else
        {
            NX_ERROR(this, "Failed to share OpenGL context");
            threadGlCtx.reset();
            return;
        }

        QOpenGLFunctions* funcs = QOpenGLContext::currentContext()->functions();
        funcs->glGenTextures(1, &textureId);
        if (textureId == 0)
        {
            NX_WARNING(this, "Failed to allocate GL texture");
            return;
        }

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
    }

    ~DecoderData() override
    {
        frameSize = {};
        surface = {};

        if (surfaceTexture.isValid())
            surfaceTexture.callMethod<void>("release");
        surfaceTexture = {};

        if (textureId != 0)
        {
            QOpenGLFunctions* funcs = QOpenGLContext::currentContext()->functions();
            funcs->glDeleteTextures(1, &textureId);
            textureId = 0;
        }
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

    FboTextureHolder textureFromFrame(const AVFrame* frame);

    bool initialized = false;
    QJniObject surface;
    QJniObject surfaceTexture;
    QSize frameSize;

    std::unique_ptr<QOpenGLShaderProgram> program;
    std::unique_ptr<QOpenGLContext> threadGlCtx;
    std::unique_ptr<QOffscreenSurface> offscreenSurface;

    GLuint textureId = 0;
    FboManager fboManager;
};

FboTextureHolder DecoderData::renderFrameToFbo()
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
            funcs->glGetBooleanv(GL_STENCIL_TEST, &stencilTestEnabled);
            funcs->glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
            funcs->glGetBooleanv(GL_SCISSOR_TEST, &scissorTestEnabled);
            funcs->glGetBooleanv(GL_BLEND, &blendEnabled);
            #if 0 //< No need to save/restore FBO binding because this context is not used anywhere else.
                GLuint prevFbo = 0;
                funcs->glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*) &prevFbo);
            #endif // 0

            CHECK_GL_ERROR

            if (stencilTestEnabled)
                funcs->glDisable(GL_STENCIL_TEST);
            if (depthTestEnabled)
                funcs->glDisable(GL_DEPTH_TEST);
            if (scissorTestEnabled)
                funcs->glDisable(GL_SCISSOR_TEST);
            if (blendEnabled)
                funcs->glDisable(GL_BLEND);

            fbo.bind();

            funcs->glViewport(0, 0, frameSize.width(), frameSize.height());
            CHECK_GL_ERROR

            program->bind();
            CHECK_GL_ERROR
            program->enableAttributeArray(0);
            CHECK_GL_ERROR
            program->enableAttributeArray(1);
            CHECK_GL_ERROR
            program->setUniformValue("texMatrix", getTransformMatrix());
            CHECK_GL_ERROR

            funcs->glActiveTexture(GL_TEXTURE0);
            funcs->glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);

            funcs->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, g_vertex_data);
            funcs->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, g_texture_data);

            funcs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            program->disableAttributeArray(0);
            program->disableAttributeArray(1);

            funcs->glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
            CHECK_GL_ERROR

            fbo.release();

            // Restore state of rendering context.
            #if 0 //< No need to save/restore FBO binding because this context is not used anywhere else.
                funcs->glBindFramebuffer(GL_FRAMEBUFFER, prevFbo); // sets both READ and DRAW
            #endif // 0

            if (stencilTestEnabled)
                funcs->glEnable(GL_STENCIL_TEST);
            if (depthTestEnabled)
                funcs->glEnable(GL_DEPTH_TEST);
            if (scissorTestEnabled)
                funcs->glEnable(GL_SCISSOR_TEST);
            if (blendEnabled)
                funcs->glEnable(GL_BLEND);

            funcs->glFlush();
            funcs->glFinish();

            static_cast<QOpenGLExtensions*>(funcs)->flushShared();
        });
}

void DecoderData::createGlResources()
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

FboTextureHolder DecoderData::textureFromFrame(const AVFrame* frame)
{
    if (!frame->data[3])
    {
        NX_WARNING(this, "No MediaCodec buffer found in output frame");
        return {};
    }

    if (av_mediacodec_release_buffer((AVMediaCodecBuffer*) frame->data[3], /*render*/ 1) < 0)
    {
        NX_WARNING(this, "Failed to render MediaCodec buffer");
        return {};
    }

    FboTextureHolder textureHolder;

    if (textureId == 0)
    {
        NX_WARNING(this, "Failed to allocate GL texture");
        return {};
    }

    const QSize incomingSize(frame->width, frame->height);
    if (!incomingSize.isEmpty() && incomingSize != frameSize)
    {
        frameSize = incomingSize;
        fboManager.init(frameSize);
    }

    if (threadGlCtx)
        textureHolder = renderFrameToFbo();

    return textureHolder;
}

class AndroidVideoApiEntry: public VideoApiRegistry::Entry
{
public:
    virtual AVHWDeviceType deviceType() const override
    {
        return AV_HWDEVICE_TYPE_MEDIACODEC;
    }

    virtual nx::media::VideoFramePtr makeFrame(
        const AVFrame* frame,
        std::shared_ptr<VideoApiDecoderData> decoderData) const override
    {
        if (!frame)
            return {};

        if (!NX_ASSERT(frame->format == AV_PIX_FMT_MEDIACODEC, "frame->format is %1", frame->format))
            return {};

        auto data = std::dynamic_pointer_cast<DecoderData>(decoderData);

        const qint64 startTimeMs = frame->pts == DATETIME_INVALID
            ? DATETIME_INVALID
            : frame->pts / 1000; //< Convert usec to msec.

        FboTextureHolder textureHolder = data->textureFromFrame(frame);

        if (textureHolder.isNull())
        {
            NX_WARNING(this, "Failed to render frame to FBO");
            return {};
        }

        auto result = std::make_shared<VideoFrame>(
            std::make_unique<TextureBuffer>(
                std::move(textureHolder),
                QVideoFrameFormat(data->frameSize, QVideoFrameFormat::Format_BGRX8888)));

        result->setStartTime(startTimeMs);

        return result;
    }

    virtual std::string device(QRhi* rhi) const override
    {
        NX_ASSERT(rhi && rhi->backend() == QRhi::Implementation::OpenGLES2);

        return {};
    }

    virtual std::shared_ptr<VideoApiDecoderData> createDecoderData(
        QRhi* rhi,
        const QSize&) const override
    {
        return std::make_shared<DecoderData>(rhi);
    }

    virtual nx::media::ffmpeg::HwVideoDecoder::InitFunc initFunc(
        QRhi* rhi,
        std::shared_ptr<VideoApiDecoderData> decoderData) const override
    {
        if (!NX_ASSERT(rhi->backend() == QRhi::OpenGLES2))
            return {};

        return
            [decoderData=std::dynamic_pointer_cast<DecoderData>(decoderData)](
                AVHWDeviceContext* deviceContext) -> bool
            {
                if (deviceContext->type != AV_HWDEVICE_TYPE_MEDIACODEC)
                {
                    NX_WARNING(NX_SCOPE_TAG,
                        "Unexpected device context type: %1", deviceContext->type);
                    return false;
                }

                auto hwctx = (AVMediaCodecContext*) deviceContext->hwctx;
                if (!hwctx)
                {
                    NX_WARNING(NX_SCOPE_TAG, "MediaCodec context not allocated");
                    return false;
                }

                hwctx->surface = decoderData->surface.object<jobject>();
                NX_DEBUG(NX_SCOPE_TAG, "MediaCodec context with surface %1", hwctx->surface);
                return true;
            };
    }
};

} // namespace

VideoApiRegistry::Entry* getMediaCodecApi()
{
    static AndroidVideoApiEntry apiEntry;
    return &apiEntry;
}

} // namespace nx::media
