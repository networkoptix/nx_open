#include "android_decoder.h"

#include <utils/thread/mutex.h>
#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#include "abstract_resource_allocator.h"
#include <utils/media/h264_utils.h>
#include <QtGui/QOpenGLContext>
#include <QOpenGLContext>
#include <qopenglfunctions.h>
#include <qopenglshaderprogram.h>
#include <qopenglframebufferobject.h>

#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>

#if defined(Q_OS_ANDROID)

namespace nx {
namespace media {

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

class TextureBuffer : public QAbstractVideoBuffer
{
public:
    TextureBuffer (uint id):
        QAbstractVideoBuffer(GLTextureHandle),
        m_id(id),
        //m_javaDecoder(javaDecoder),
        m_updated(false)
    {
    }

    ~TextureBuffer()
    {
        //m_javaDecoder.callMethod<void>("onCurrentBufferRendered");
    }

    MapMode mapMode() const {
        return NotMapped;
    }
    uchar *map(MapMode, int *, int *) {
        return 0;
    }
    void unmap()
    {
    }

    QVariant handle() const
    {
        return m_id;
    }

private:
    GLuint m_id;
    //GLuint m_textId;
    //QAndroidJniObject m_javaDecoder;
    mutable bool m_updated;
};


// ---------------------- QnTextureBuffer ----------------------
/*
class TextureBuffer : public QAbstractVideoBuffer
{
public:
    TextureBuffer (uint id, const QAndroidJniObject& javaDecoder):
        QAbstractVideoBuffer(GLTextureHandle),
        m_id(id),
        m_javaDecoder(javaDecoder),
        m_updated(false)
    {
    }

    ~TextureBuffer()
    {
        //m_javaDecoder.callMethod<void>("onCurrentBufferRendered");
    }

    MapMode mapMode() const {
        return NotMapped;
    }
    uchar *map(MapMode, int *, int *) {
        return 0;
    }
    void unmap()
    {
    }
    QVariant handle() const
    {
        if (!m_updated)
        //if (m_textId == -1)
        {
            m_updated = true;

            QOpenGLContext* context = QOpenGLContext::currentContext();
            if (!context)
                return QVariant();
            //context->functions()->glGenTextures(m_textId, 1);
            //m_javaDecoder.callMethod<void>("attachToGLContext", "(I)Z", (jint) m_id);
            int gg = 4;
            //m_javaDecoder.callMethod<void>("updateTexImage");
        }
        return QVariant::fromValue<uint>(m_id);
    }

private:
    GLuint m_id;
    //GLuint m_textId;
    QAndroidJniObject m_javaDecoder;
    mutable bool m_updated;
};
*/
// --------------------------------------------------------------------------------------------------


void fillInputBuffer(JNIEnv *env, jobject thiz, jobject buffer, jlong srcDataPtr, jint dataSize)
{
    void *bytes = env->GetDirectBufferAddress(buffer);
    void* srcData = (void*) srcDataPtr;
    //int dataSize = env->GetDirectBufferCapacity(buffer);
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
        glSurfaceTexture(0),
        glExternalTexture(0),
        initialized(false),
        javaDecoder("com/networkoptix/nxwitness/media/QnMediaDecoder"),
        m_fbo(nullptr),
        m_program(nullptr)

	{
        registerNativeMethods();
	}

    ~AndroidDecoderPrivate()
    {
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
    void createGLResources();

    qint64 frameNumber;
    GLuint glSurfaceTexture;
    GLuint glExternalTexture;
    std::unique_ptr<QOpenGLContext> glContext;
    std::unique_ptr<QOffscreenSurface> fakeSurface;
    bool initialized;
    QAndroidJniObject javaDecoder;
    AbstractResourceAllocator* allocator;
    QSize frameSize;

    QMutex m_mutex;
    QOpenGLFramebufferObject *m_fbo;
    QOpenGLShaderProgram *m_program;
};

void AndroidDecoderPrivate::renderFrameToFbo()
{
    QMutexLocker locker(&m_mutex);

    createGLResources();

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

    if (stencilTestEnabled)
        glDisable(GL_STENCIL_TEST);
    if (depthTestEnabled)
        glDisable(GL_DEPTH_TEST);
    if (scissorTestEnabled)
        glDisable(GL_SCISSOR_TEST);
    if (blendEnabled)
        glDisable(GL_BLEND);

    m_fbo->bind();

    glViewport(0, 0, frameSize.width(), frameSize.height());

    m_program->bind();
    m_program->enableAttributeArray(0);
    m_program->enableAttributeArray(1);
    m_program->setUniformValue("frameTexture", GLuint(0));
    m_program->setUniformValue("texMatrix", getTransformMatrix());

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, g_vertex_data);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, g_texture_data);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    m_program->disableAttributeArray(0);
    m_program->disableAttributeArray(1);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    m_fbo->release();

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

void AndroidDecoderPrivate::createGLResources()
{
    if (!m_fbo)
    {
        m_fbo = new QOpenGLFramebufferObject(frameSize);
        //auto texId = allocator->newGlTexture();
        //m_fbo = new QOpenGLFramebufferObject(frameSize, texId);
    }

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

        QOpenGLShader *fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, m_program);
        fragmentShader->compileSourceCode("#extension GL_OES_EGL_image_external : require \n" \
                                          "varying highp vec2         textureCoords; \n" \
                                          "uniform samplerExternalOES frameTexture; \n" \
                                          "void main() \n" \
                                          "{ \n" \
                                          "    gl_FragColor = texture2D(frameTexture, textureCoords); \n" \
                                          "}\n");
        m_program->addShader(fragmentShader);

        m_program->bindAttributeLocation("vertexCoordsArray", 0);
        m_program->bindAttributeLocation("textureCoordArray", 1);
        m_program->link();
    }
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
    QOpenGLContext* sharedContext = allocator->getGlContext();
    if (sharedContext)
    {
        d->glContext.reset(new QOpenGLContext());
        d->glContext->setShareContext(sharedContext);
        d->glContext->setFormat(sharedContext->format());
        //d->glContext->setScreen(sharedContext->screen());

        //d->glContext->moveToThread(this);

        // We need a non-visible surface to make current in the other thread
                // and QWindows must be created and managed on the GUI thread.
        d->fakeSurface.reset(new QOffscreenSurface());
        d->fakeSurface->setFormat(d->glContext->format());
        d->fakeSurface->create();

        if (d->glContext->create())
        {
            //d->glContext->makeCurrent(sharedContext->surface());
            d->glContext->setShareContext(sharedContext);
            d->glContext->makeCurrent(d->fakeSurface.get());

            QOpenGLContext* context = QOpenGLContext::currentContext();
            d->glContext->functions()->glGenTextures(1, &d->glSurfaceTexture);
            d->glExternalTexture = d->allocator->newGlTexture();
        }
    }

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
        d->initialized = d->javaDecoder.callMethod<jboolean>("init", "(Ljava/lang/String;III)Z",
                         jCodecName.object<jstring>(), d->frameSize.width(), d->frameSize.height(),
                         d->glSurfaceTexture
                         //d->glExternalTexture
        );
        if (!d->initialized)
            return 0; //< wait for I frame
    }


    jlong outFrameNum = d->javaDecoder.callMethod<jlong>("decodeFrame", "(JIJ)J",
                                                         (jlong) frame->data(),
                                                         (jint) frame->dataSize(),
                                                         (jlong) ++d->frameNumber);
    if (outFrameNum <= 0)
        return outFrameNum;

    // got frame

    d->renderFrameToFbo();

    //QAbstractVideoBuffer* buffer = new TextureBuffer (d->m_fbo->texture());
    //QVideoFrame* videoFrame = new QVideoFrame(buffer, d->frameSize, QVideoFrame::Format_BGR32);

    QImage image = d->m_fbo->toImage();
    QImage image2 = image.convertToFormat(QImage::Format_RGB32);
    qDebug() << "fbo info: size=" << image.size() << "format=" << image.format();
    QVideoFrame* videoFrame = new QVideoFrame(image2);
    result->reset(videoFrame);

    return (int) outFrameNum - 1;
}


} // namespace media
} // namespace nx

#endif // #defined(Q_OS_ANDROID)
