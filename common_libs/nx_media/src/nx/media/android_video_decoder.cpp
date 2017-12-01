#include "android_video_decoder.h"
#if defined(Q_OS_ANDROID)

#include <deque>

#include <QtCore/QElapsedTimer>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLFramebufferObject>

#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>
#include <QtOpenGL/QtOpenGL>
#include <QtCore/QCache>
#include <QtCore/QMap>

#include <nx/utils/thread/mutex.h>
#include <utils/media/h264_utils.h>
#include <nx/utils/log/log.h>

#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>

#define USE_GUI_RENDERING
#define USE_SHARED_CTX

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

bool isValidFrameSize(const QSize& size)
{
    static const auto kMinimumFrameSize = QSize(64, 64);
    return size.width() >= kMinimumFrameSize.width()
        && size.height() >= kMinimumFrameSize.height();
}

/**
 * Convert codec from ffmpeg enum to Android codec string representation.
 * Only codeccs listed below are supported by AndroidVideoDecoder.
 */
static QString codecToString(AVCodecID codecId)
{
    switch(codecId)
    {
        case AV_CODEC_ID_H265:
            return lit("video/hevc");
        case AV_CODEC_ID_H264:
            return lit("video/avc");
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
            return lit("video/3gpp");
        case AV_CODEC_ID_MPEG4:
            return lit("video/mp4v-es");
        case AV_CODEC_ID_MPEG2VIDEO:
            return lit("video/mpeg2");
        case AV_CODEC_ID_VP8:
            return lit("video/x-vnd.on2.vp8");
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
        qDebug() << lit("gl error %1").arg(error); \

} // namespace

//-------------------------------------------------------------------------------------------------

typedef std::shared_ptr<QOpenGLFramebufferObject> FboPtr;

class FboManager
{
public:
    FboManager(const QSize& frameSize):
        m_frameSize(frameSize),
        m_index(0)
    {
    }

    FboPtr getFbo()
    {
        #ifdef USE_GUI_RENDERING
            while (m_data.size() < 3)
                m_data.push_back(FboPtr(new QOpenGLFramebufferObject(m_frameSize)));
            return m_data[m_index++ % m_data.size()];
            //if (!m_fbo)
            //    m_fbo = FboPtr(new QOpenGLFramebufferObject(m_frameSize));
            //return m_fbo;
        #else
            return FboPtr(new QOpenGLFramebufferObject(m_frameSize));
        #endif
    }
private:
    FboPtr m_fbo;
    std::vector<FboPtr> m_data;
    QSize m_frameSize;
    int m_index;
};

//-------------------------------------------------------------------------------------------------

class TextureBuffer: public QAbstractVideoBuffer
{
public:
    TextureBuffer(const FboPtr& fbo, std::shared_ptr<AndroidVideoDecoderPrivate> owner)
    :
        QAbstractVideoBuffer(GLTextureHandle),
        m_fbo(fbo),
        m_owner(owner)
    {
    }

    ~TextureBuffer()
    {
    }

    virtual MapMode mapMode() const override
    {
        return NotMapped;
    }

    virtual uchar* map(MapMode, int*, int*) override
    {
        return 0;
    }

    virtual void unmap() override
    {
    }

    virtual QVariant handle() const override;

private:
    mutable FboPtr m_fbo;
    std::weak_ptr<AndroidVideoDecoderPrivate> m_owner;
};

//-------------------------------------------------------------------------------------------------
// AndroidVideoDecoderPrivate

class AndroidVideoDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(AndroidVideoDecoder)
    AndroidVideoDecoder *q_ptr;

public:
    AndroidVideoDecoderPrivate(const ResourceAllocatorPtr& allocator):
        frameNumber(0),
        initialized(false),
        javaDecoder("com/networkoptix/nxwitness/media/QnVideoDecoder"),
        allocator(allocator),
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

        QAndroidJniEnvironment env;
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

        QAndroidJniEnvironment env;
        jclass objectClass = env->GetObjectClass(javaDecoder.object<jobject>());
        env->RegisterNatives(
            objectClass,
            methods,
            sizeof(methods) / sizeof(methods[0]));
        env->DeleteLocalRef(objectClass);
    }

    FboPtr renderFrameToFbo();
    void createGlResources();

    static void addMaxResolutionIfNeeded(const AVCodecID codec);

private:
    static QMap<AVCodecID, QSize> maxResolutions;
    static QMutex maxResolutionsMutex;

    qint64 frameNumber;
    bool initialized;
    QAndroidJniObject javaDecoder;
    ResourceAllocatorPtr allocator;
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

FboPtr AndroidVideoDecoderPrivate::renderFrameToFbo()
{

    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();

    createGlResources();
    FboPtr fbo = fboManager->getFbo();

    CHECK_GL_ERROR

    updateTexImage();

    // save current render states
    GLboolean stencilTestEnabled;
    GLboolean depthTestEnabled;
    GLboolean scissorTestEnabled;
    GLboolean blendEnabled;
    glGetBooleanv(GL_STENCIL_TEST, &stencilTestEnabled);
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    glGetBooleanv(GL_SCISSOR_TEST, &scissorTestEnabled);
    glGetBooleanv(GL_BLEND, &blendEnabled);
    //GLuint prevFbo = 0;
    //funcs->glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *) &prevFbo);

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

    // restore render states
    //funcs->glBindFramebuffer(GL_FRAMEBUFFER, prevFbo); // sets both READ and DRAW
    if (stencilTestEnabled)
        glEnable(GL_STENCIL_TEST);
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    if (scissorTestEnabled)
        glEnable(GL_SCISSOR_TEST);
    if (blendEnabled)
        glEnable(GL_BLEND);

    if (threadGlCtx)
    {
        // We used decoder thread to render. Flush everything to the texture.
        glFlush();
        glFinish();
    }

    return fbo;
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
    const ResourceAllocatorPtr& allocator, const QSize& /*resolution*/)
    :
    AbstractVideoDecoder(),
    d(new AndroidVideoDecoderPrivate(allocator))
{
    #if defined(USE_SHARED_CTX) && !defined(USE_GUI_RENDERING)
        QOpenGLContext* sharedContext = QOpenGLContext::globalShareContext();
        if (sharedContext)
        {
            d->threadGlCtx.reset(new QOpenGLContext());
            d->threadGlCtx->setShareContext(sharedContext);
            d->threadGlCtx->setFormat(sharedContext->format());

            if (d->threadGlCtx->create() && d->threadGlCtx->shareContext())
            {
                NX_DEBUG(this, lit("Using shared openGL ctx"));
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
    #endif
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
        QAndroidJniObject jCodecName = QAndroidJniObject::fromString(codecMimeType);
        QAndroidJniObject javaDecoder("com/networkoptix/nxwitness/media/QnVideoDecoder");
        jint maxWidth = javaDecoder.callMethod<jint>(
            "maxDecoderWidth", "(Ljava/lang/String;)I", jCodecName.object<jstring>());
        jint maxHeight = javaDecoder.callMethod<jint>(
            "maxDecoderHeight", "(Ljava/lang/String;)I", jCodecName.object<jstring>());
        const QSize maxSize{maxWidth, maxHeight};
        if (maxSize.isEmpty())
        {
            // NOTE: Zeroes come from JNI in case the Java class was not loaded due to some issue.
            NX_WARNING(typeid(AndroidVideoDecoderPrivate),
                lm("ERROR: Android Video Decoder failed to report max resolution for codec %1")
                .arg(codecMimeType));
        }
        else
        {
            NX_INFO(typeid(AndroidVideoDecoderPrivate),
                lm("Maximum hardware decoder resolution: %1 for codec %2")
                .arg(maxSize).arg(codecMimeType));
            maxResolutions[codec] = maxSize;
        }
    }
}

bool AndroidVideoDecoder::isCompatible(
    const AVCodecID codec, const QSize& resolution, bool allowOverlay)
{
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
            lm("Codec for %1 is not compatible with resolution %2 because max is %3")
            .arg(codecToString(codec)).arg(resolution).arg(maxSize));
        return false;
    }

    NX_DEBUG(typeid(AndroidVideoDecoder), lm("Codec %1 is compatible with resolution %2")
        .arg(codecToString(codec)).arg(resolution));

    return true;
}

QSize AndroidVideoDecoder::maxResolution(const AVCodecID codec)
{
    AndroidVideoDecoderPrivate::addMaxResolutionIfNeeded(codec);

    QMutexLocker lock(&AndroidVideoDecoderPrivate::maxResolutionsMutex);
    return AndroidVideoDecoderPrivate::maxResolutions[codec]; //< Return empty QSize if not found.
}

int AndroidVideoDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result)
{
    QElapsedTimer tm;
    tm.restart();
    if (!d->initialized)
    {
        if (!frame)
            return 0;

        d->frameSize = QSize(frame->width, frame->height);
        if (!isValidFrameSize(d->frameSize))
            d->frameSize = nx::media::AbstractVideoDecoder::mediaSizeFromRawData(frame);
        if (d->frameSize.isEmpty())
            return 0; //< Wait for I frame to be able to extract data from the binary stream.

        QString codecName = codecToString(frame->compressionType);
        QAndroidJniObject jCodecName = QAndroidJniObject::fromString(codecName);
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

    FboPtr fboToRender;
    #if !defined(USE_GUI_RENDERING)
        if (d->threadGlCtx)
        {
            fboToRender = d->renderFrameToFbo();
        }
        else
        {
            d->allocator->execAtGlThread(
                [&fboToRender, this](void*)
                {
                    fboToRender = d->renderFrameToFbo();
                },
                nullptr);
        }
    #endif

    NX_VERBOSE(this, lit("got frame num %1 decode time1=%2 time2=%3").arg(outFrameNum).arg(time1).arg(tm.elapsed()));

    QAbstractVideoBuffer* buffer = new TextureBuffer (std::move(fboToRender), d);
    QVideoFrame* videoFrame = new QVideoFrame(buffer, d->frameSize, QVideoFrame::Format_BGR32);

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

AbstractVideoDecoder::Capabilities AndroidVideoDecoder::::capabilities() const
{
    return Capability::hardwareAccelerated;
}

QVariant TextureBuffer::handle() const
{
    if (!m_fbo)
    {
        if (auto ptr = m_owner.lock())
            m_fbo = ptr->renderFrameToFbo();
    }
    return m_fbo ? m_fbo->texture() : 0;
}


} // namespace media
} // namespace nx

#endif // defined(Q_OS_ANDROID)
