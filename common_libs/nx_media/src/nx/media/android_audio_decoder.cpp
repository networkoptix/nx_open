#include "android_audio_decoder.h"

#include <deque>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLFramebufferObject>

#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>
#include <QtOpenGL/QtOpenGL>
#include <QMap>

#include <utils/thread/mutex.h>

#if defined(Q_OS_ANDROID)

#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>

#include "abstract_resource_allocator.h"

namespace nx {
namespace media {

namespace {

    QString codecToString(CodecID codecId)
    {
        switch(codecId)
        {
            case CODEC_ID_AAC:
                return lit("audio/mp4a-latm");
            case CODEC_ID_PCM_ALAW:
                return lit("audio/g711-alaw");
            case CODEC_ID_PCM_MULAW:
                return lit("audio/g711-mlaw");
            case CODEC_ID_MP2:
            case CODEC_ID_MP3:
                return lit("audio/mpeg");
            case CODEC_ID_VORBIS:
                return lit("audio/vorbis");
            default:
                return QString();
        }
    }

    void fillInputBuffer(JNIEnv *env, jobject thiz, jobject buffer, jlong srcDataPtr, jint dataSize);
    void readOutputBuffer(JNIEnv *env, jobject thiz, jlong cObject, jobject buffer, jint bufferSize);
}

// ------------------------------------------------------------------------------


// ------------------------- AndroidAudioDecoderPrivate -------------------------

class AndroidAudioDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(AndroidAudioDecoder)
    AndroidAudioDecoder *q_ptr;
public:
    AndroidAudioDecoderPrivate():
        initialized(false),
        javaDecoder("com/networkoptix/nxwitness/media/QnMediaDecoder")
    {
        registerNativeMethods();
    }

    ~AndroidAudioDecoderPrivate()
    {
        javaDecoder.callMethod<void>("releaseDecoder");
    }

    void registerNativeMethods()
    {
        using namespace std::placeholders;

        JNINativeMethod methods[] {
            {"fillInputBuffer", "(Ljava/nio/ByteBuffer;JI)V", reinterpret_cast<void *>(nx::media::fillInputBuffer)},
            { "readOutputBuffer", "(Ljava/nio/ByteBuffer;JJI)V", reinterpret_cast<void *>(nx::media::readOutputBuffer) }
        };

        QAndroidJniEnvironment env;
        jclass objectClass = env->GetObjectClass(javaDecoder.object<jobject>());
        env->RegisterNatives(
            objectClass,
            methods,
            sizeof(methods) / sizeof(methods[0]));
        env->DeleteLocalRef(objectClass);
    }

    void readOutputBuffer(const void* buffer, int bufferSize)
    {
        audioFrame.reset(new nx::AudioFrame());
        audioFrame->data.write((const char*) buffer, bufferSize);
    }

private:
    bool initialized;
    QAndroidJniObject javaDecoder;
    AudioFramePtr audioFrame;
};

namespace {

void fillInputBuffer(JNIEnv *env, jobject thiz, jobject buffer, jlong srcDataPtr, jint dataSize)
{
    Q_UNUSED(thiz);
    void* bytes = env->GetDirectBufferAddress(buffer);
    void* srcData = (void*)srcDataPtr;
    memcpy(bytes, srcData, dataSize);
}

void readOutputBuffer(JNIEnv *env, jobject thiz, jlong cObject, jobject buffer, jint bufferSize)
{
    Q_UNUSED(thiz);
    AndroidAudioDecoderPrivate* decoder = static_cast<AndroidAudioDecoderPrivate*> ((void*) cObject);
    void* bytes = env->GetDirectBufferAddress(buffer);
    decoder->readOutputBuffer(bytes, bufferSize);
}

}

// ---------------------- AndroidAudioDecoder ----------------------

AndroidAudioDecoder::AndroidAudioDecoder():
    AbstractAudioDecoder(),
    d_ptr(new AndroidAudioDecoderPrivate())

{
}

AndroidAudioDecoder::~AndroidAudioDecoder()
{
}

bool AndroidAudioDecoder::isCompatible(const CodecID codec)
{
    static QMap<CodecID, QSize> maxDecoderSize;
    static QMutex mutex;

    QMutexLocker lock(&mutex);

    const QString codecMimeType = codecToString(codec);
    return !codecMimeType.isEmpty();
}

AudioFramePtr AndroidAudioDecoder::decode(const QnConstCompressedAudioDataPtr& frame)
{
    Q_D(AndroidAudioDecoder);

    if (!frame)
        return AudioFramePtr();

    if (!d->initialized)
    {

        QString codecName = codecToString(frame->compressionType);
        QAndroidJniObject jCodecName = QAndroidJniObject::fromString(codecName);
        d->initialized = d->javaDecoder.callMethod<jboolean>(
            "init", "(Ljava/lang/String;)Z",
            jCodecName.object<jstring>());
        if (!d->initialized)
            return AudioFramePtr();
    }

    d->audioFrame.reset();
    d->javaDecoder.callMethod<jboolean>(
        "decodeAudio", "(JIJ)Z",
        (jlong) frame->data(),
        (jint) frame->dataSize(),
        (jlong) (void*) this);

    if (d->audioFrame)
    {
        d->audioFrame->context = frame->context;
        d->audioFrame->timestampUsec = frame->timestamp;
    }
    
    return d->audioFrame;
}

} // namespace media
} // namespace nx

#endif // #defined(Q_OS_ANDROID)
