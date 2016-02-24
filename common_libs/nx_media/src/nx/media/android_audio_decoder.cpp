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
        magic(1234567),
        initialized(false),
        javaDecoder("com/networkoptix/nxwitness/media/QnAudioDecoder")
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
            { "readOutputBuffer", "(JLjava/nio/ByteBuffer;I)V", reinterpret_cast<void *>(nx::media::readOutputBuffer) }
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
    int magic;
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

bool AndroidAudioDecoder::decode(const QnConstCompressedAudioDataPtr& frame, AudioFramePtr* const outFrame)
{
    Q_D(AndroidAudioDecoder);

    if (!frame)
        return false;

    if (!d->initialized)
    {
        QnCodecAudioFormat audioFormat(frame->context);
        QString codecName = codecToString(frame->compressionType);
        QAndroidJniObject jCodecName = QAndroidJniObject::fromString(codecName);

        d->initialized = d->javaDecoder.callMethod<jboolean>(
            "init", "(Ljava/lang/String;IIJI)Z",
            jCodecName.object<jstring>(),
            audioFormat.sampleRate(),
            audioFormat.channels(),
            (jlong) frame->context->getExtradata(),
            frame->context->getExtradataSize());
        if (!d->initialized)
            return false;
    }

    d->audioFrame.reset();
    jlong outFrameTimestamp = d->javaDecoder.callMethod<jlong>(
        "decodeFrame", "(JIJJ)J",
        (jlong) frame->data(),
        (jint) frame->dataSize(),
        (jlong) frame->timestamp,
        (jlong) (const void*) d_ptr.data());

    if (outFrameTimestamp < 0)
        return false;

    if (d->audioFrame)
    {
        d->audioFrame->timestampUsec = outFrameTimestamp;
        d->audioFrame->context = frame->context;
        *outFrame = std::move(d->audioFrame);
    }
    
    return true;
}

} // namespace media
} // namespace nx

#endif // #defined(Q_OS_ANDROID)
