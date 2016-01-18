#include "android_decoder.h"

#include <utils/thread/mutex.h>
#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#include "abstract_resource_allocator.h"
#include <utils/media/h264_utils.h>
#include <QtGui/QOpenGLContext>
#include <QOpenGLContext>
#include <QtGui/QOpenGLFunctions>

#if defined(Q_OS_ANDROID)

namespace nx {
namespace media {


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
        initialized(false),
        javaDecoder("com/networkoptix/nxwitness/media/QnMediaDecoder")
	{
        registerNativeMethods();
	}

    ~AndroidDecoderPrivate()
    {
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

    const QnCompressedVideoData* tempData;
    qint64 frameNumber;
    GLuint glSurfaceTexture;
    std::unique_ptr<QOpenGLContext> glContext;
    bool initialized;
    QAndroidJniObject javaDecoder;
    AbstractResourceAllocator* allocator;
    QSize frameSize;
};

// ---------------------- QnTextureBuffer ----------------------

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
        m_javaDecoder.callMethod<void>("onCurrentBufferRendered");
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
        if (!m_updated) {
            m_updated = true;
            m_javaDecoder.callMethod<void>("updateTexImage");
        }
        return QVariant::fromValue<uint>(m_id);
    }

private:
    GLuint m_id;
    QAndroidJniObject m_javaDecoder;
    mutable bool m_updated;
};



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
        d->glContext->setFormat(sharedContext->format());
        d->glContext->setShareContext(sharedContext);
        if (d->glContext->create())
        {
            d->glContext->setScreen(sharedContext->screen());
            d->glContext->functions()->glGenTextures(1, &d->glSurfaceTexture);
        }
    }

    //if (d->allocator)
    //    d->glTexture = d->allocator->newGlTexture();
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
                         jCodecName.object<jstring>(), d->frameSize.width(), d->frameSize.height(), d->glSurfaceTexture);
        if (!d->initialized)
            return 0; //< wait for I frame
    }


    d->tempData = frame.get();
    jlong outFrameNum = d->javaDecoder.callMethod<jlong>("decodeFrame", "(JIJ)J",
                                                         (jlong) frame->data(),
                                                         (jint) frame->dataSize(),
                                                         (jlong) ++d->frameNumber);
    if (outFrameNum <= 0)
        return outFrameNum;

    // got frame

    //m_javaDecoder.callMethod<void>("updateTexImage");

    QAbstractVideoBuffer* buffer = new TextureBuffer (d->glSurfaceTexture, d->javaDecoder);
    QVideoFrame* videoFrame = new QVideoFrame(buffer, d->frameSize, QVideoFrame::Format_ARGB32);
    result->reset(videoFrame);
    return (int) outFrameNum - 1;
}


} // namespace media
} // namespace nx

#endif // #defined(Q_OS_ANDROID)
