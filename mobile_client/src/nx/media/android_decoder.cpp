#include "android_decoder.h"

#include <utils/thread/mutex.h>
#include "abstract_resource_allocator.h"
#include <utils/media/h264_utils.h>
#include <QtGui/QOpenGLContext>
#include <QOpenGLContext>
#include <qopenglfunctions.h>
#include <qopenglshaderprogram.h>
#include <qopenglframebufferobject.h>

#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>
#include <QtGui/qopengl.h>
#include <QCache>

#if defined(Q_OS_ANDROID)

#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>

namespace nx {
namespace media {

namespace {
    static const int kFrameQueueMaxSize = 32; //< max java decoder internal queue size
}

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


// --------------------------------------------------------------------------------------------------

typedef std::shared_ptr<QOpenGLFramebufferObject> FboPtr;

class FboManager
{
public:
    FboManager(const QSize& frameSize):
        m_frameSize(frameSize)
    {

    }

    FboPtr getFbo()
    {
        for(const auto& fbo: m_data)
        {
            if (fbo.use_count() == 1)
                return fbo; // this object is free
        }
        FboPtr newFbo(new QOpenGLFramebufferObject(m_frameSize));
        m_data.push_back(newFbo);
        return newFbo;
    }
private:
    std::vector<FboPtr> m_data;
    QSize m_frameSize;
};

// --------------------------------------------------------------------------------------------------

class TextureBuffer: public QAbstractVideoBuffer
{
public:
    TextureBuffer (const FboPtr& fbo):
        QAbstractVideoBuffer(GLTextureHandle),
        m_fbo(fbo)
    {
    }

    virtual MapMode mapMode() const override { return NotMapped;}
    virtual uchar *map(MapMode, int *, int *) override { return 0; }
    virtual void unmap() override {}
    virtual QVariant handle() const override { return m_fbo->texture(); }

private:
    FboPtr m_fbo;
};

// --------------------------------------------------------------------------------------------------


void fillInputBuffer(JNIEnv *env, jobject thiz, jobject buffer, jlong srcDataPtr, jint dataSize)
{
    void *bytes = env->GetDirectBufferAddress(buffer);
    void* srcData = (void*) srcDataPtr;
    memcpy(bytes, srcData, dataSize);
}

// ------------------------- AndroidDecoderPrivate -------------------------

class AndroidDecoderPrivate : public QObject
{
    Q_DECLARE_PUBLIC(AndroidDecoder)
    AndroidDecoder *q_ptr;
public:
    AndroidDecoderPrivate():
        frameNumber(0),
        initialized(false),
        javaDecoder("com/networkoptix/nxwitness/media/QnMediaDecoder"),
        m_program(nullptr),
        frameNumToPtsCache(kFrameQueueMaxSize)

	{
        registerNativeMethods();
	}

    ~AndroidDecoderPrivate()
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
            {"fillInputBuffer", "(Ljava/nio/ByteBuffer;JI)V", reinterpret_cast<void *>(nx::media::fillInputBuffer)}
        };

        QAndroidJniEnvironment env;
        jclass objectClass = env->GetObjectClass(javaDecoder.object<jobject>());
        env->RegisterNatives(objectClass,
                             methods,
                             sizeof(methods) / sizeof(methods[0]));
        env->DeleteLocalRef(objectClass);
    }

    void renderFrameToFbo();
    FboPtr createGLResources();

private:
    qint64 frameNumber;
    bool initialized;
    QAndroidJniObject javaDecoder;
    AbstractResourceAllocator* allocator;
    QSize frameSize;

    std::unique_ptr<FboManager> m_fboManager;
    FboPtr m_currentFbo;

    QMutex m_mutex;
    QOpenGLShaderProgram *m_program;

    QCache<int, qint64> frameNumToPtsCache;
};

void renderFrameToFbo(void* opaque)
{
    AndroidDecoderPrivate* d = static_cast<AndroidDecoderPrivate*>(opaque);
    d->renderFrameToFbo();
}

void AndroidDecoderPrivate::renderFrameToFbo()
{
    QMutexLocker locker(&m_mutex);

    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();

    m_currentFbo = createGLResources();
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

    updateTexImage();
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());


    // save current render states
    GLboolean stencilTestEnabled;
    GLboolean depthTestEnabled;
    GLboolean scissorTestEnabled;
    GLboolean blendEnabled;
    glGetBooleanv(GL_STENCIL_TEST, &stencilTestEnabled);
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    glGetBooleanv(GL_SCISSOR_TEST, &scissorTestEnabled);
    glGetBooleanv(GL_BLEND, &blendEnabled);

    if (stencilTestEnabled)
        glDisable(GL_STENCIL_TEST);
    if (depthTestEnabled)
        glDisable(GL_DEPTH_TEST);
    if (scissorTestEnabled)
        glDisable(GL_SCISSOR_TEST);
    if (blendEnabled)
        glDisable(GL_BLEND);

    m_currentFbo->bind();

    glViewport(0, 0, frameSize.width(), frameSize.height());
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

    m_program->bind();
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());
    m_program->enableAttributeArray(0);
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());
    m_program->enableAttributeArray(1);
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());
    m_program->setUniformValue("frameTexture", GLuint(0));
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());
    m_program->setUniformValue("texMatrix", getTransformMatrix());
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, g_vertex_data);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, g_texture_data);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    m_program->disableAttributeArray(0);
    m_program->disableAttributeArray(1);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    if (funcs->glGetError())
        qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

    m_currentFbo->release();

    // restore render states
    if (stencilTestEnabled)
        glEnable(GL_STENCIL_TEST);
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    if (scissorTestEnabled)
        glEnable(GL_SCISSOR_TEST);
    if (blendEnabled)
        glEnable(GL_BLEND);

}

FboPtr AndroidDecoderPrivate::createGLResources()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    QOpenGLFunctions *funcs = ctx->functions();

    if (!m_fboManager)
        m_fboManager.reset(new FboManager(frameSize));
    FboPtr fbo = m_fboManager->getFbo();

    if (!m_program)
    {
        m_program = new QOpenGLShaderProgram;

        QOpenGLShader *vertexShader = new QOpenGLShader(QOpenGLShader::Vertex, m_program);
        vertexShader->compileSourceCode("attribute highp vec4 vertexCoordsArray; \n" \
                                        "attribute highp vec2 textureCoordArray; \n" \
                                        "uniform   highp mat4 texMatrix; \n" \
                                        "varying   highp vec2 textureCoords; \n" \
                                        "void main(void) \n" \
                                        "{ \n" \
                                        "    gl_Position = vertexCoordsArray; \n" \
                                        "    textureCoords = (texMatrix * vec4(textureCoordArray, 0.0, 1.0)).xy; \n" \
                                        "}\n");
        m_program->addShader(vertexShader);
        if (funcs->glGetError())
            qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

        QOpenGLShader *fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, m_program);
        fragmentShader->compileSourceCode("#extension GL_OES_EGL_image_external : require \n" \
                                          "varying highp vec2         textureCoords; \n" \
                                          "uniform samplerExternalOES frameTexture; \n" \
                                          "void main() \n" \
                                          "{ \n" \
                                          "    gl_FragColor = texture2D(frameTexture, textureCoords); \n" \
                                          "}\n");
        m_program->addShader(fragmentShader);
        if (funcs->glGetError())
            qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());

        m_program->bindAttributeLocation("vertexCoordsArray", 0);
        m_program->bindAttributeLocation("textureCoordArray", 1);
        m_program->link();
        if (funcs->glGetError())
            qDebug() << "gl error: " << funcs->glGetString(funcs->glGetError());
    }
    return fbo;
}

// ---------------------- AndroidDecoder ----------------------

AndroidDecoder::AndroidDecoder():
	AbstractVideoDecoder(),
    d_ptr(new AndroidDecoderPrivate())

{
}

AndroidDecoder::~AndroidDecoder()
{
}

void AndroidDecoder::setAllocator(AbstractResourceAllocator* allocator )
{
    Q_D(AndroidDecoder);
    d->allocator = allocator;
}

bool AndroidDecoder::isCompatible(const QnConstCompressedVideoDataPtr& frame)
{
    return frame->compressionType == CODEC_ID_H264;
}

QString codecToString(CodecID codecId)
{
    switch(codecId)
    {
        case CODEC_ID_H264:
            return lit("video/avc");
        default:
            return QString();
    }
}

int AndroidDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QnVideoFramePtr* result)
{
    Q_D(AndroidDecoder);

    if (!d->initialized)
    {
        extractSpsPps(frame, &d->frameSize, nullptr);
        if (d->frameSize.isNull())
            return 0; //< wait for I frame

        QString codecName = codecToString(frame->compressionType);
        QAndroidJniObject jCodecName = QAndroidJniObject::fromString(codecName);
        d->initialized = d->javaDecoder.callMethod<jboolean>("init", "(Ljava/lang/String;II)Z",
                         jCodecName.object<jstring>(), d->frameSize.width(), d->frameSize.height()
        );
        if (!d->initialized)
            return 0; //< wait for I frame
    }


    d->frameNumToPtsCache.insert(d->frameNumber, new qint64(frame->timestamp));
    jlong outFrameNum = d->javaDecoder.callMethod<jlong>("decodeFrame", "(JIJ)J",
                                                         (jlong) frame->data(),
                                                         (jint) frame->dataSize(),
                                                         (jlong) ++d->frameNumber); //< put input frames in range [1..N]
    if (outFrameNum <= 0)
        return outFrameNum;

    // got frame

    d->allocator->execAtGlThread(renderFrameToFbo, d);
    if (!d->m_currentFbo)
        return 0; //< can't decode now. skip frame

    QAbstractVideoBuffer* buffer = new TextureBuffer (std::move(d->m_currentFbo));
    QVideoFrame* videoFrame = new QVideoFrame(buffer, d->frameSize, QVideoFrame::Format_BGR32);
    qint64* pts = d->frameNumToPtsCache.object(outFrameNum);
    if (pts)
        videoFrame->setStartTime(*pts);
    d->frameNumToPtsCache.remove(outFrameNum);

    result->reset(videoFrame);
    return (int) outFrameNum - 1; //< convert range [1..N] to [0..N]
}

} // namespace media
} // namespace nx

#endif // #defined(Q_OS_ANDROID)
