// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "android_video_decoder.h"
#if defined(Q_OS_ANDROID)

#include <deque>
#include <queue>

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
#include <QtMultimedia/private/qhwvideobuffer_p.h>
#include <QtMultimedia/QVideoFrameFormat>
#include <QtOpenGL/QOpenGLFramebufferObject>
#include <QtOpenGL/QOpenGLShaderProgram>

#include <media/filters/h264_mp4_to_annexb.h>
#include <nx/media/h264_utils.h>
#include <nx/media/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "fbo_manager.h"

namespace nx::media {

namespace {

static const qint64 kDecodeOneFrameTimeout = 1000 * 33;

// ATTENTION: These constants are coupled with the ones in QnVideoDecoder.java.
static const int kNoInputBuffers = -7;
static const int kCodecFailed = -8;

// Some decoders may have no input buffers left because of long decoding time.
// We will try to skip single output frame to clear space in input buffers.
static const int kDequeueInputBufferRetyrCounter = 3;

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
 * Only codeccs listed below are supported by AndroidVideoDecoder.
 */
static QString codecToString(AVCodecID codecId)
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
        default:
            return QString();
    }
}

static void fillInputBuffer(
    JNIEnv *env, jobject /*thiz*/, jobject buffer, jlong srcDataPtr, jint dataSize, jint capacity)
{
    void* bytes = env->GetDirectBufferAddress(buffer);
    void* srcData = (void*) srcDataPtr;
    if (capacity < dataSize)
        qWarning() << "fillInputBuffer: capacity less then dataSize." << capacity << "<" << dataSize;
    memcpy(bytes, srcData, qMin(dataSize, capacity));
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

    virtual quint64 textureHandle(QRhi*, int plane) const override
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
// AndroidVideoDecoderPrivate

class AndroidVideoDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(AndroidVideoDecoder)
    AndroidVideoDecoder *q_ptr;

public:
    AndroidVideoDecoderPrivate(const RenderContextSynchronizerPtr& synchronizer):
        frameNumber(0),
        initialized(false),
        javaDecoder("com/nxvms/mobile/media/QnVideoDecoder"),
        synchronizer(synchronizer),
        program(nullptr)
    {
        registerNativeMethods();
    }

    ~AndroidVideoDecoderPrivate()
    {
        javaDecoder.callMethod<void>("releaseDecoder");
    }

    void releaseSurface()
    {
        javaDecoder.callMethod<void>("releaseSurface", "()V");
    }

    void updateTexImage()
    {
        javaDecoder.callMethod<void>("updateTexImage");
    }

    QMatrix4x4 getTransformMatrix()
    {
        QMatrix4x4 matrix;

        QJniEnvironment env;
        jfloatArray array = env->NewFloatArray(16);
        javaDecoder.callMethod<void>("getTransformMatrix", "([F)V", array);
        env->GetFloatArrayRegion(array, 0, 16, matrix.data());
        env->DeleteLocalRef(array);

        return matrix;
    }

    void registerNativeMethods()
    {
        JNINativeMethod methods[] {
            {"fillInputBuffer", "(Ljava/nio/ByteBuffer;JII)V",
                reinterpret_cast<void*>(nx::media::fillInputBuffer)}
        };

        QJniEnvironment env;
        jclass objectClass = env->GetObjectClass(javaDecoder.object<jobject>());
        env->RegisterNatives(
            objectClass,
            methods,
            sizeof(methods) / sizeof(methods[0]));
        env->DeleteLocalRef(objectClass);
    }

    FboTextureHolder renderFrameToFbo();
    void createGlResources();

    static void addMaxResolutionIfNeeded(const AVCodecID codec);

private:
    static QMap<AVCodecID, QSize> maxResolutions;
    static QMutex maxResolutionsMutex;

    qint64 frameNumber;
    H2645Mp4ToAnnexB filter;
    bool initialized;
    QJniObject javaDecoder;
    RenderContextSynchronizerPtr synchronizer;
    QSize frameSize;

    QOpenGLShaderProgram *program;

    typedef std::pair<int, qint64> PtsData;
    std::deque<PtsData> frameNumToPtsCache;

    std::unique_ptr<QOpenGLContext> threadGlCtx;
    std::unique_ptr<QOffscreenSurface> offscreenSurface;

    GLuint textureId = 0;
    FboManager fboManager;
};

QMap<AVCodecID, QSize> AndroidVideoDecoderPrivate::maxResolutions;
QMutex AndroidVideoDecoderPrivate::maxResolutionsMutex;

FboTextureHolder AndroidVideoDecoderPrivate::renderFrameToFbo()
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
        });
}

void AndroidVideoDecoderPrivate::createGlResources()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    QOpenGLFunctions *funcs = ctx->functions();

    if (!program)
    {
        program = new QOpenGLShaderProgram();

        QOpenGLShader *vertexShader = new QOpenGLShader(QOpenGLShader::Vertex, program);
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

        QOpenGLShader *fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, program);
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
    d(new AndroidVideoDecoderPrivate(synchronizer))
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
    if (d->initialized)
    {
        d->releaseSurface();
        glDeleteTextures(1, &d->textureId);
    }
}

void AndroidVideoDecoderPrivate::addMaxResolutionIfNeeded(const AVCodecID codec)
{
    QMutexLocker lock(&maxResolutionsMutex);

    const QString codecMimeType = codecToString(codec);
    if (codecMimeType.isEmpty())
        return;

    if (!maxResolutions.contains(codec))
    {
        QJniObject jCodecName = QJniObject::fromString(codecMimeType);
        QJniObject javaDecoder("com/nxvms/mobile/media/QnVideoDecoder");
        jint maxWidth = javaDecoder.callMethod<jint>(
            "maxDecoderWidth", "(Ljava/lang/String;)I", jCodecName.object<jstring>());
        jint maxHeight = javaDecoder.callMethod<jint>(
            "maxDecoderHeight", "(Ljava/lang/String;)I", jCodecName.object<jstring>());
        const QSize maxSize{maxWidth, maxHeight};
        if (maxSize.isEmpty())
        {
            // NOTE: Zeroes come from JNI in case the Java class was not loaded due to some issue.
            NX_WARNING(typeid(AndroidVideoDecoderPrivate),
                nx::format("ERROR: Android Video Decoder failed to report max resolution for codec %1")
                .arg(codecMimeType));
        }
        else
        {
            NX_INFO(typeid(AndroidVideoDecoderPrivate),
                nx::format("Maximum hardware decoder resolution: %1 for codec %2")
                .arg(maxSize).arg(codecMimeType));
            maxResolutions[codec] = maxSize;
        }
    }
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

    AndroidVideoDecoderPrivate::addMaxResolutionIfNeeded(codec);

    QSize maxSize;
    {
        QMutexLocker lock(&AndroidVideoDecoderPrivate::maxResolutionsMutex);
        maxSize = AndroidVideoDecoderPrivate::maxResolutions[codec];
    }
    if (maxSize.isEmpty())
        return false;

    if (resolution.width() > maxSize.width() || resolution.height() > maxSize.height())
    {
        NX_WARNING(typeid(AndroidVideoDecoder),
            nx::format("Codec for %1 is not compatible with resolution %2 because max is %3")
            .arg(codecToString(codec)).arg(resolution).arg(maxSize));
        return false;
    }

    NX_DEBUG(typeid(AndroidVideoDecoder), nx::format("Codec %1 is compatible with resolution %2")
        .arg(codecToString(codec)).arg(resolution));

    return true;
}

QSize AndroidVideoDecoder::maxResolution(const AVCodecID codec)
{
    AndroidVideoDecoderPrivate::addMaxResolutionIfNeeded(codec);

    QMutexLocker lock(&AndroidVideoDecoderPrivate::maxResolutionsMutex);
    return AndroidVideoDecoderPrivate::maxResolutions[codec]; //< Return empty QSize if not found.
}

int AndroidVideoDecoder::decode(const QnConstCompressedVideoDataPtr& frameSrc, VideoFramePtr* result)
{
    QnConstCompressedVideoDataPtr frame;
    if (frameSrc)
    {
        // The filter keeps same frame in case of filtering is not required or
        // it is not the H264/H265 codec.
        frame = std::dynamic_pointer_cast<const QnCompressedVideoData>(
            d->filter.processData(frameSrc));
        if (!frame)
        {
            NX_WARNING(this, "Failed to convert h264/h265 video to Annex B format");
            return 0;
        }
    }

    QElapsedTimer tm;
    tm.restart();
    if (!d->initialized)
    {
        if (!frame)
            return 0;

        d->frameSize = getFrameSize(frame.get());
        if (d->frameSize.isEmpty())
            return 0; //< Wait for I frame to be able to extract data from the binary stream.

        d->fboManager.init(d->frameSize);

        glGenTextures(1, &d->textureId);
        QString codecName = codecToString(frame->compressionType);
        QJniObject jCodecName = QJniObject::fromString(codecName);
        d->initialized = d->javaDecoder.callMethod<jboolean>(
            "init", "(Ljava/lang/String;III)Z",
            jCodecName.object<jstring>(),
            (jint) d->textureId,
            d->frameSize.width(),
            d->frameSize.height());
        if (!d->initialized)
        {
            d->releaseSurface();
            glDeleteTextures(1, &d->textureId);
            return 0; //< wait for I frame
        }
    }

    jlong outFrameNum = 0;
    int retryCounter = 0;
    do
    {
        if (frame)
        {
            ++d->frameNumber; //< Put input frames in range [1..N].
            d->frameNumToPtsCache.push_back(
                AndroidVideoDecoderPrivate::PtsData(d->frameNumber, frame->timestamp));
            outFrameNum = d->javaDecoder.callMethod<jlong>(
                "decodeFrame", "(JIJ)J",
                (jlong) frame->data(),
                (jint) frame->dataSize(),
                (jlong) d->frameNumber);
            if (outFrameNum == kNoInputBuffers)
            {
                if (d->javaDecoder.callMethod<jlong>(
                    "flushFrame", "(J)J", (jlong) kDecodeOneFrameTimeout) <= 0)
                {
                    break;
                }
            }
            else if (outFrameNum == kCodecFailed)
            {
                // Subsequent call to decode() will reinitialize javaDecoder.
                d->initialized = false;
                d->releaseSurface();
                glDeleteTextures(1, &d->textureId);
                return 0;
            }
        }
        else
        {
            outFrameNum = d->javaDecoder.callMethod<jlong>("flushFrame", "(J)J", (jlong) 0);
        }
    } while (outFrameNum == kNoInputBuffers && ++retryCounter < kDequeueInputBufferRetyrCounter);

    if (outFrameNum <= 0)
        return outFrameNum;

    auto time1 = tm.elapsed();

    // Got a frame.

    qint64 frameTime = -1;

    while (!d->frameNumToPtsCache.empty() && d->frameNumToPtsCache.front().first < outFrameNum)
        d->frameNumToPtsCache.pop_front(); //< In case of decoder skipped some input frames
    if (!d->frameNumToPtsCache.empty() && d->frameNumToPtsCache.front().first == outFrameNum)
    {
        qint64 pts = d->frameNumToPtsCache.front().second;
        frameTime = pts / 1000; //< usec to msec
        d->frameNumToPtsCache.pop_front();
    }

    // If we didn't find a timestamp for the frame, that usually means that android decoder
    // provided a frame out of order. In this case we should just skip it.
    if (frameTime < 0)
        return 0;

    FboTextureHolder textureHolder;

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
        return 0;

    NX_VERBOSE(this, "Got frame num %1 decode time1=%2 time2=%3",
        outFrameNum, time1, tm.elapsed());

    auto videoFrame = new VideoFrame(std::make_unique<TextureBuffer>(
        std::move(textureHolder),
        QVideoFrameFormat(d->frameSize, QVideoFrameFormat::Format_BGRX8888)));
    videoFrame->setStartTime(frameTime);

    result->reset(videoFrame);
    return (int) outFrameNum - 1; //< convert range [1..N] to [0..N]
}

AbstractVideoDecoder::Capabilities AndroidVideoDecoder::capabilities() const
{
    return Capability::hardwareAccelerated;
}

} // namespace nx::media

#endif // defined(Q_OS_ANDROID)
