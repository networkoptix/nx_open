#include "android_audio_decoder.h"

#if defined(Q_OS_ANDROID)

#include <deque>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLFramebufferObject>

#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>
#include <QtOpenGL/QtOpenGL>
#include <QMap>

#include <nx/utils/thread/mutex.h>

#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>

#include "abstract_render_context_synchronizer.h"

namespace nx {
namespace media {

namespace {

    QString codecToString(AVCodecID codecId)
    {
        switch(codecId)
        {
            case AV_CODEC_ID_AAC:
                return "audio/mp4a-latm";
            case AV_CODEC_ID_PCM_ALAW:
                return "audio/g711-alaw";
            case AV_CODEC_ID_PCM_MULAW:
                return "audio/g711-mlaw";
            case AV_CODEC_ID_MP2:
            case AV_CODEC_ID_MP3:
                return "audio/mpeg";
            case AV_CODEC_ID_VORBIS:
                return "audio/vorbis";
            default:
                return QString();
        }
    }

    void fillInputBuffer(JNIEnv *env, jobject thiz, jobject buffer, jlong srcDataPtr, jint dataSize, jint capacity);
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
            {"fillInputBuffer", "(Ljava/nio/ByteBuffer;JII)V", reinterpret_cast<void *>(nx::media::fillInputBuffer)},
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
    bool initialized;
    QAndroidJniObject javaDecoder;
    AudioFramePtr audioFrame;
};

namespace {

void fillInputBuffer(JNIEnv *env, jobject thiz, jobject buffer, jlong srcDataPtr, jint dataSize, jint capacity)
{
    Q_UNUSED(thiz);
    void* bytes = env->GetDirectBufferAddress(buffer);
    void* srcData = (void*)srcDataPtr;
    if (capacity < dataSize)
        qWarning() << "fillInputBuffer: capacity less then dataSize." << capacity << "<" << dataSize;
    memcpy(bytes, srcData, qMin(capacity, dataSize));
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

bool AndroidAudioDecoder::isDecoderCompatibleToPlatform()
{
    return QAndroidJniObject::callStaticMethod<jboolean>(
        "com/networkoptix/nxwitness/media/QnAudioDecoder",
        "isDecoderCompatibleToPlatform");
}

bool AndroidAudioDecoder::isCompatible(const AVCodecID codec)
{
    static QMap<AVCodecID, QSize> maxDecoderSize;
    static QMutex mutex;

    QMutexLocker lock(&mutex);

    if (!isDecoderCompatibleToPlatform())
        return false;

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
