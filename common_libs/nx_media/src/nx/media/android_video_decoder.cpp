#include "android_video_decoder.h"

#if defined(Q_OS_ANDROID)

#include <deque>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLFramebufferObject>

#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>
#include <QtOpenGL/QtOpenGL>
#include <QCache>
#include <QMap>

#include <nx/utils/thread/mutex.h>
#include <utils/media/h264_utils.h>
#include <nx/utils/log/log.h>

#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>

#include "abstract_resource_allocator.h"


#define USE_GUI_RENDERING
#define USE_SHARED_CTX

namespace nx {
namespace media {

namespace {

    static const int kNoInputBuffers = -7;
    static const qint64 kDecodeOneFrameTimeout = 1000 * 33;

    // some decoders may have not input buffers left because of long decoding time
    // We will try to skip single output frame to clear space in input buffers
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

    QString codecToString(CodecID codecId)
    {
        switch(codecId)
        {
            case CODEC_ID_H264:
                return lit("video/avc");
            case CODEC_ID_H263:
                return lit("video/3gpp");
            case CODEC_ID_MPEG4:
                return lit("video/mp4v-es");
            case CODEC_ID_MPEG2VIDEO:
                return lit("video/mpeg2");
            case CODEC_ID_VP8:
                return lit("video/x-vnd.on2.vp8");
            default:
                return QString();
        }
    }

    void fillInputBuffer(JNIEnv *env, jobject thiz, jobject buffer, jlong srcDataPtr, jint dataSize, jint capacity)
    {
        Q_UNUSED(thiz);
        void* bytes = env->GetDirectBufferAddress(buffer);
        void* srcData = (void*) srcDataPtr;
        if (capacity < dataSize)
            qWarning() << "fillInputBuffer: capacity less then dataSize." << capacity << "<" << dataSize;
        memcpy(bytes, srcData, qMin(dataSize, capacity));
    }
}

// --------------------------------------------------------------------------------------------------

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

// --------------------------------------------------------------------------------------------------

class TextureBuffer: public QAbstractVideoBuffer
{
public:
    TextureBuffer (const FboPtr& fbo, std::shared_ptr<AndroidVideoDecoderPrivate> owner):
        QAbstractVideoBuffer(GLTextureHandle),
        m_fbo(fbo),
        m_owner(owner)
    {

    }

    ~TextureBuffer()
    {

    }

    virtual MapMode mapMode() const override { return NotMapped;}
    virtual uchar *map(MapMode, int *, int *) override { return 0; }
    virtual void unmap() override {}
    virtual QVariant handle() const override;

private:
    mutable FboPtr m_fbo;
    std::weak_ptr<AndroidVideoDecoderPrivate> m_owner;
};

// --------------------------------------------------------------------------------------------------



// ------------------------- AndroidVideoDecoderPrivate -------------------------

class AndroidVideoDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(AndroidVideoDecoder)
    AndroidVideoDecoder *q_ptr;
public:
    AndroidVideoDecoderPrivate():
        frameNumber(0),
        initialized(false),
        javaDecoder("com/networkoptix/nxwitness/media/QnVideoDecoder"),
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
        using namespace std::placeholders;

        JNINativeMethod methods[] {
            {"fillInputBuffer", "(Ljava/nio/ByteBuffer;JII)V", reinterpret_cast<void *>(nx::media::fillInputBuffer)}
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
private:
    qint64 frameNumber;
    bool initialized;
    QAndroidJniObject javaDecoder;
    AbstractResourceAllocator* allocator;
    QSize frameSize;

    std::unique_ptr<FboManager> fboManager;
    QOpenGLShaderProgram *program;

    typedef std::pair<int, qint64> PtsData;
    std::deque<PtsData> frameNumToPtsCache;

    std::unique_ptr<QOpenGLContext> threadGlCtx;
    std::unique_ptr<QOffscreenSurface> offscreenSurface;
};

FboPtr AndroidVideoDecoderPrivate::renderFrameToFbo()
{
    QElapsedTimer tm;
    tm.restart();

    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();

    createGlResources();
    FboPtr fbo = fboManager->getFbo();

    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

    auto t1 = tm.elapsed();

    updateTexImage();

    auto t2 = tm.elapsed();

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

    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

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
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

    program->bind();
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());
    program->enableAttributeArray(0);
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());
    program->enableAttributeArray(1);
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());
    program->setUniformValue("frameTexture", GLuint(0));
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());
    program->setUniformValue("texMatrix", getTransformMatrix());
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, g_vertex_data);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, g_texture_data);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    program->disableAttributeArray(0);
    program->disableAttributeArray(1);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

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

    auto t3 = tm.elapsed();

    if (threadGlCtx)
    {
        // we used decoder thread to render. flush everythink to the texture
        glFlush();
        glFinish();
    }

    auto t4 = tm.elapsed();
    //NX_LOG(lit("render t1=%1 t2=%2 t3=%3 t4=%4").arg(t1).arg(t2).arg(t3).arg(t4), cl_logINFO);

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
            "attribute highp vec4 vertexCoordsArray; \n" \
            "attribute highp vec2 textureCoordArray; \n" \
            "uniform   highp mat4 texMatrix; \n" \
            "varying   highp vec2 textureCoords; \n" \
            "void main(void) \n" \
            "{ \n" \
            "    gl_Position = vertexCoordsArray; \n" \
            "    textureCoords = (texMatrix * vec4(textureCoordArray, 0.0, 1.0)).xy; \n" \
            "}\n");
        program->addShader(vertexShader);
        if (funcs->glGetError())
            qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

        QOpenGLShader *fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, program);
        fragmentShader->compileSourceCode(
            "#extension GL_OES_EGL_image_external : require \n" \
            "varying highp vec2         textureCoords; \n" \
            "uniform samplerExternalOES frameTexture; \n" \
            "void main() \n" \
            "{ \n" \
            "    gl_FragColor = texture2D(frameTexture, textureCoords); \n" \
            "}\n");
        program->addShader(fragmentShader);
        if (funcs->glGetError())
            qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

        program->bindAttributeLocation("vertexCoordsArray", 0);
        program->bindAttributeLocation("textureCoordArray", 1);
        program->link();
        if (funcs->glGetError())
            qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());
    }
}

// ---------------------- AndroidVideoDecoder ----------------------

AndroidVideoDecoder::AndroidVideoDecoder():
    AbstractVideoDecoder(),
    d(new AndroidVideoDecoderPrivate())

{
}

AndroidVideoDecoder::~AndroidVideoDecoder()
{
}

void AndroidVideoDecoder::setAllocator(AbstractResourceAllocator* allocator)
{
    d->allocator = allocator;

#if defined(USE_SHARED_CTX) && !defined(USE_GUI_RENDERING)
    QOpenGLContext* sharedContext = QOpenGLContext::globalShareContext();
    if (sharedContext)
    {
        d->threadGlCtx.reset(new QOpenGLContext());
        d->threadGlCtx->setShareContext(sharedContext);
        d->threadGlCtx->setFormat(sharedContext->format());

        if (d->threadGlCtx->create() && d->threadGlCtx->shareContext())
        {
            NX_LOG(lit("Using shared openGL ctx"), cl_logINFO);
            d->offscreenSurface.reset(new QOffscreenSurface());
            d->offscreenSurface->setFormat(d->threadGlCtx->format());
            d->offscreenSurface->create();

            d->threadGlCtx->makeCurrent(d->offscreenSurface.get());
        }
        else {
            d->threadGlCtx.reset();
        }
    }
#endif
}

bool AndroidVideoDecoder::isCompatible(const CodecID codec, const QSize& resolution)
{
    static QMap<CodecID, QSize> maxDecoderSize;
    static QMutex mutex;

    QMutexLocker lock(&mutex);

    const QString codecMimeType = codecToString(codec);
    if (codecMimeType.isEmpty())
        return false;

    if (!maxDecoderSize.contains(codec))
    {
        QAndroidJniObject jCodecName = QAndroidJniObject::fromString(codecMimeType);
        QAndroidJniObject javaDecoder("com/networkoptix/nxwitness/media/QnVideoDecoder");
        jint maxWidth = javaDecoder.callMethod<jint>("maxDecoderWidth", "(Ljava/lang/String;)I", jCodecName.object<jstring>());
        jint maxHeight = javaDecoder.callMethod<jint>("maxDecoderHeight", "(Ljava/lang/String;)I", jCodecName.object<jstring>());
        QSize size(maxWidth, maxHeight);
        maxDecoderSize[codec] = size;
        qDebug() << "Maximum hardware decoder resolution:" << size << "for codec" << codecMimeType;
    }
    const QSize maxSize = maxDecoderSize[codec];
    return resolution.width() <= maxSize.width() && resolution.height() <= maxSize.height();
}

int AndroidVideoDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result)
{
    QElapsedTimer tm;
    tm.restart();
    if (!d->initialized)
    {
        if (!frame)
            return 0;
        extractSpsPps(frame, &d->frameSize, nullptr);
        if (d->frameSize.isNull())
            return 0; //< wait for I frame

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
            d->frameNumToPtsCache.push_back(AndroidVideoDecoderPrivate::PtsData(d->frameNumber, frame->timestamp));
            outFrameNum = d->javaDecoder.callMethod<jlong>(
                "decodeFrame", "(JIJ)J",
                (jlong) frame->data(),
                (jint) frame->dataSize(),
                (jlong) ++d->frameNumber); //< put input frames in range [1..N]
        }
        else {
            outFrameNum = d->javaDecoder.callMethod<jlong>("flushFrame", "(J)J", 0);
        }

        if (outFrameNum == kNoInputBuffers)
        {
            if (d->javaDecoder.callMethod<jlong>("flushFrame", "(J)J", kDecodeOneFrameTimeout) <= 0)
                break;
        }
    } while (outFrameNum == kNoInputBuffers && ++retryCounter < kDequeueInputBufferRetyrCounter);

    if (outFrameNum <= 0)
        return outFrameNum;

    auto time1 = tm.elapsed();

    // got frame

    FboPtr fboToRender;
#if defined(USE_GUI_RENDERING)
#else
    if (d->threadGlCtx)
        fboToRender = d->renderFrameToFbo();
    else
        d->allocator->execAtGlThread([&fboToRender, this](void*)
        {
            fboToRender = d->renderFrameToFbo();
        }, nullptr);
#endif

    //NX_LOG(lit("--got frame num %1 decode time1=%2 time2=%3").arg(outFrameNum).arg(time1).arg(tm.elapsed()), cl_logINFO);

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
    return (int)outFrameNum - 1; //< convert range [1..N] to [0..N]
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

#endif // Q_OS_ANDROID
