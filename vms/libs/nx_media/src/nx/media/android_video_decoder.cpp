// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "android_video_decoder.h"
#if defined(Q_OS_ANDROID)

#include <deque>
#include <queue>

#include <QtCore/QCache>
#include <QtCore/QElapsedTimer>
#include <QtCore/QJniEnvironment>
#include <QtCore/QJniObject>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtMultimedia/QVideoFrameFormat>
#include <QtMultimedia/private/qabstractvideobuffer_p.h>
#include <QtOpenGL/QOpenGLFramebufferObject>
#include <QtOpenGL/QOpenGLShaderProgram>

#include <media/filters/h264_mp4_to_annexb.h>
#include <nx/media/h264_utils.h>
#include <nx/media/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace media {

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

class FboManager
{
    static constexpr int kFboPoolSize = 4;

public:
    struct FboHolder
    {
        FboHolder(std::weak_ptr<QOpenGLFramebufferObject> fbo, FboManager* manager):
            fbo(fbo),
            manager(manager)
        {
        }

        ~FboHolder()
        {
            const auto fboPtr = fbo.lock();
            if (!fboPtr)
                return;

            NX_MUTEX_LOCKER lock(&manager->m_mutex);
            manager->m_fbos.erase(
                std::find(manager->m_fbos.begin(), manager->m_fbos.end(), fboPtr));
            manager->m_fbosToDelete.push(fboPtr);
        }

        std::weak_ptr<QOpenGLFramebufferObject> fbo;
        FboManager* manager = nullptr;
    };

    using FboPtr = std::shared_ptr<FboHolder>;

    FboManager(const QSize& frameSize):
        m_frameSize(frameSize)
    {
    }

    FboPtr getFbo()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        // It looks like the video surface we use releases the presented QVideoFrame too early
        // (before it is actually rendered). If we delete it immediately, we'll get screen
        // blinking. Here we give the FBO a slight delay defore the deletion.
        while (m_fbosToDelete.size() > 1)
            m_fbosToDelete.pop();

        m_fbos.emplace_back(new QOpenGLFramebufferObject(m_frameSize));
        return std::make_shared<FboHolder>(m_fbos.back(), this);
    }

private:
    std::vector<std::shared_ptr<QOpenGLFramebufferObject>> m_fbos;
    std::queue<std::shared_ptr<QOpenGLFramebufferObject>> m_fbosToDelete;
    Mutex m_mutex;
    QSize m_frameSize;
};

//-------------------------------------------------------------------------------------------------

class TextureBuffer: public QAbstractVideoBuffer
{
public:
    TextureBuffer(
        FboManager::FboPtr&& fbo,
        std::shared_ptr<AndroidVideoDecoderPrivate> owner)
        :
        QAbstractVideoBuffer(QVideoFrame::RhiTextureHandle),
        m_fbo(std::move(fbo)),
        m_owner(owner)
    {
    }

    virtual QVideoFrame::MapMode mapMode() const override
    {
        return QVideoFrame::NotMapped;
    }

    virtual MapData map(QVideoFrame::MapMode) override
    {
        return {};
    }

    virtual void unmap() override
    {
    }

    virtual quint64 textureHandle(int plane) const override;

private:
    mutable FboManager::FboPtr m_fbo;
    std::weak_ptr<AndroidVideoDecoderPrivate> m_owner;
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
        javaDecoder("com/networkoptix/nxwitness/media/QnVideoDecoder"),
        synchronizer(synchronizer),
        program(nullptr)
    {
        registerNativeMethods();
    }

    ~AndroidVideoDecoderPrivate()
    {
        javaDecoder.callMethod<void>("releaseDecoder");
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

    FboManager::FboPtr renderFrameToFbo();
    void createGlResources();

    static void addMaxResolutionIfNeeded(const AVCodecID codec);

private:
    static QMap<AVCodecID, QSize> maxResolutions;
    static QMutex maxResolutionsMutex;

    qint64 frameNumber;
    bool initialized;
    QJniObject javaDecoder;
    RenderContextSynchronizerPtr synchronizer;
    QSize frameSize;

    std::unique_ptr<FboManager> fboManager;
    QOpenGLShaderProgram *program;

    typedef std::pair<int, qint64> PtsData;
    std::deque<PtsData> frameNumToPtsCache;

    std::unique_ptr<QOpenGLContext> threadGlCtx;
    std::unique_ptr<QOffscreenSurface> offscreenSurface;
};

QMap<AVCodecID, QSize> AndroidVideoDecoderPrivate::maxResolutions;
QMutex AndroidVideoDecoderPrivate::maxResolutionsMutex;

FboManager::FboPtr AndroidVideoDecoderPrivate::renderFrameToFbo()
{
    QOpenGLFunctions* funcs = QOpenGLContext::currentContext()->functions();

    createGlResources();
    FboManager::FboPtr fboPtr = fboManager->getFbo();
    auto fbo = fboPtr->fbo.lock();

    CHECK_GL_ERROR

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

    fbo->bind();

    glViewport(0, 0, frameSize.width(), frameSize.height());
    CHECK_GL_ERROR

    program->bind();
    CHECK_GL_ERROR
    program->enableAttributeArray(0);
    CHECK_GL_ERROR
    program->enableAttributeArray(1);
    CHECK_GL_ERROR
    program->setUniformValue("frameTexture", GLuint(0));
    CHECK_GL_ERROR
    program->setUniformValue("texMatrix", getTransformMatrix());
    CHECK_GL_ERROR

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, g_vertex_data);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, g_texture_data);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    program->disableAttributeArray(0);
    program->disableAttributeArray(1);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    CHECK_GL_ERROR

    fbo->release();

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

    return fboPtr;
}

void AndroidVideoDecoderPrivate::createGlResources()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    QOpenGLFunctions *funcs = ctx->functions();

    if (!fboManager)
        fboManager.reset(new FboManager(frameSize));

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
        QJniObject javaDecoder("com/networkoptix/nxwitness/media/QnVideoDecoder");
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
        H2645Mp4ToAnnexB filter;
        frame = std::dynamic_pointer_cast<const QnCompressedVideoData>(
            filter.processData(frameSrc));
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

        QString codecName = codecToString(frame->compressionType);
        QJniObject jCodecName = QJniObject::fromString(codecName);
        d->initialized = d->javaDecoder.callMethod<jboolean>(
            "init", "(Ljava/lang/String;II)Z",
            jCodecName.object<jstring>(),
            d->frameSize.width(),
            d->frameSize.height());
        if (!d->initialized)
            return 0; //< wait for I frame
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

    FboManager::FboPtr fboToRender;
    if (d->threadGlCtx)
    {
        fboToRender = d->renderFrameToFbo();
    }
    else
    {
        d->synchronizer->execInRenderContext(
            [&fboToRender, this](void*)
            {
                fboToRender = d->renderFrameToFbo();
            },
            nullptr);
    }

    NX_VERBOSE(this, nx::format("got frame num %1 decode time1=%2 time2=%3").args(
        outFrameNum, time1, tm.elapsed()));

    auto videoFrame = new VideoFrame(
        new TextureBuffer(std::move(fboToRender), d),
        QVideoFrameFormat(d->frameSize, QVideoFrameFormat::Format_BGRX8888));

    while (!d->frameNumToPtsCache.empty() && d->frameNumToPtsCache.front().first < outFrameNum)
        d->frameNumToPtsCache.pop_front(); //< In case of decoder skipped some input frames
    if (!d->frameNumToPtsCache.empty() && d->frameNumToPtsCache.front().first == outFrameNum)
    {
        qint64 pts = d->frameNumToPtsCache.front().second;
        videoFrame->setStartTime(pts / 1000); //< usec to msec
        d->frameNumToPtsCache.pop_front();
    }

    result->reset(videoFrame);
    return (int) outFrameNum - 1; //< convert range [1..N] to [0..N]
}

AbstractVideoDecoder::Capabilities AndroidVideoDecoder::capabilities() const
{
    return Capability::hardwareAccelerated;
}

quint64 TextureBuffer::textureHandle(int /*plane*/) const
{
    if (m_fbo)
    {
        if (auto fbo = m_fbo->fbo.lock())
            return fbo->texture();
    }
    return 0;
}

} // namespace media
} // namespace nx

#endif // defined(Q_OS_ANDROID)
