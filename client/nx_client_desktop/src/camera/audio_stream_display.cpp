#include "audio_stream_display.h"

#include <nx/utils/log/log.h>
#include <utils/media/audio_processor.h>

#include "decoders/audio/audio_struct.h"
#include "decoders/audio/abstract_audio_decoder.h"

#include "client/client_settings.h"
#include <nx/streaming/config.h>
#include <nx/audio/audiodevice.h>

namespace {
static const int AVCODEC_MAX_AUDIO_FRAME_SIZE = 192 * 1000;
}

QnAudioStreamDisplay::QnAudioStreamDisplay(int bufferMs, int prebufferMs):
    m_bufferMs(bufferMs),
    m_prebufferMs(prebufferMs),
    m_tooFewDataDetected(true),
    m_isFormatSupported(true),
    m_downmixing(false),
    m_forceDownmix(qnSettings->isAudioDownmixed()),
    m_sampleConvertMethod(SampleConvertMethod::none),
    m_isConvertMethodInitialized(false),
    m_decodedAudioBuffer(CL_MEDIA_ALIGNMENT, AVCODEC_MAX_AUDIO_FRAME_SIZE),
    m_startBufferingTime(AV_NOPTS_VALUE),
    m_lastAudioTime(AV_NOPTS_VALUE),
    m_audioQueueMutex(QnMutex::Recursive),
    m_blockedTimeValue(AV_NOPTS_VALUE)
{
}

QnAudioStreamDisplay::~QnAudioStreamDisplay()
{
    for (auto decoder: m_decoders)
        delete decoder;
}

int QnAudioStreamDisplay::msInBuffer() const
{
    int internalBufferSize = (m_sound ? m_sound->playTimeElapsedUsec() / 1000.0 : 0);
    int rez = msInQueue() + internalBufferSize;
    return rez;
}

qint64 QnAudioStreamDisplay::getCurrentTime() const
{
    if (m_blockedTimeValue != qint64(AV_NOPTS_VALUE))
        return m_blockedTimeValue;
    else if (m_lastAudioTime == qint64(AV_NOPTS_VALUE))
        return qint64(AV_NOPTS_VALUE);
    else
        return m_lastAudioTime - msInBuffer() * 1000ll;
}

void QnAudioStreamDisplay::blockTimeValue(qint64 value)
{
    m_blockedTimeValue = value;
}

void QnAudioStreamDisplay::unblockTimeValue()
{
    m_blockedTimeValue = qint64(AV_NOPTS_VALUE);
}

void QnAudioStreamDisplay::suspend()
{
    QnMutexLocker lock(&m_guiSync);
    if (m_sound)
    {
        m_tooFewDataDetected = true;
        m_sound->suspend();
    }
}

void QnAudioStreamDisplay::resume()
{
    QnMutexLocker lock(&m_guiSync);
    if (m_sound)
        m_sound->resume();
}

qint64 QnAudioStreamDisplay::startBufferingTime() const
{
    return m_tooFewDataDetected ? m_startBufferingTime : qint64(AV_NOPTS_VALUE);
}

bool QnAudioStreamDisplay::isFormatSupported() const
{
    if (!m_sound)
        return true; //< it is not constructed yet

    //return m_audioSound->isFormatSupported();
    return m_isFormatSupported;
}

void QnAudioStreamDisplay::clearDeviceBuffer()
{
    if (m_sound)
        m_sound->clear();
}

void QnAudioStreamDisplay::clearAudioBuffer()
{
    QnMutexLocker lock(&m_audioQueueMutex);

    while (!m_audioQueue.isEmpty())
        m_audioQueue.dequeue();

    clearDeviceBuffer();
    m_tooFewDataDetected = true;
    m_lastAudioTime = qint64(AV_NOPTS_VALUE);
}

void QnAudioStreamDisplay::enqueueData(QnCompressedAudioDataPtr data, qint64 minTime)
{
    QnMutexLocker lock(&m_audioQueueMutex);

    if (!createDecoder(data))
        return;

    m_lastAudioTime = data->timestamp;
    m_audioQueue.enqueue(data);

    while (m_audioQueue.size() > 0)
    {
        QnCompressedAudioDataPtr front = m_audioQueue.front();
        if (front->timestamp >= minTime && msInQueue() < m_bufferMs)
            break;
        m_audioQueue.dequeue();
    }

    //if (msInBuffer() >= m_bufferMs / 10)
    //    m_audioQueue.dequeue()->releaseRef();
}

bool QnAudioStreamDisplay::initFormatConvertRule(QnAudioFormat format)
{
    if (m_forceDownmix && format.channelCount() > 2)
    {
        format.setChannelCount(2);
        m_downmixing = true;
    }
    else
    {
        m_downmixing = false;
    }

    if (nx::audio::Sound::isFormatSupported(format))
        return true;

    if (format.sampleType() == QnAudioFormat::Float)
    {
        format.setSampleType(QnAudioFormat::SignedInt);
        if (nx::audio::Sound::isFormatSupported(format))
        {
            m_sampleConvertMethod = SampleConvertMethod::float2Int32;
            return true;
        }
        format.setSampleSize(16);
        if (nx::audio::Sound::isFormatSupported(format))
        {
            m_sampleConvertMethod = SampleConvertMethod::float2Int16;
            return true;
        }
    }
    else if (format.sampleSize() == 32)
    {
        format.setSampleSize(16);
        if (nx::audio::Sound::isFormatSupported(format))
        {
            m_sampleConvertMethod = SampleConvertMethod::int32ToInt16;
            return true;
        }
    }

    format.setChannelCount(2);
    if (nx::audio::Sound::isFormatSupported(format))
    {
        m_downmixing = true;
        return true;
    }
    return false; //< conversion rule not found
}

bool QnAudioStreamDisplay::createDecoder(const QnCompressedAudioDataPtr& data)
{
    if (m_decoders[data->compressionType] == nullptr)
    {
        m_decoders[data->compressionType] = QnAudioDecoderFactory::createDecoder(data);
        if (m_decoders[data->compressionType] == nullptr)
            return false;
    }
    return true;
}

bool QnAudioStreamDisplay::putData(QnCompressedAudioDataPtr data, qint64 minTime)
{
    QnMutexLocker lock(&m_audioQueueMutex);
    if (!createDecoder(data))
        return false;

    m_lastAudioTime = data->timestamp;
    static const int MAX_BUFFER_LEN = 3000;
    if (data == 0 && !m_sound) //< No need to check audio device if data=0 and no audio device.
        return false;

    // Sometimes distance between audio packets in a file is very large (may be more than
    // audio_device buffer); audio_device buffer is small, and we need to put the data from the
    // packets. To do it, we call this function with null pointer.

    int bufferSizeMs = msInBuffer();
    if (bufferSizeMs < m_bufferMs / 10)
    {
        m_tooFewDataDetected = true;
        m_startBufferingTime = data->timestamp - bufferSizeMs * 1000;
    }

    bool CanDropLateAudio = !m_sound || m_sound->state() != QAudio::State::ActiveState;
    if (CanDropLateAudio && data && data->timestamp < minTime)
    {
        clearAudioBuffer();
        m_startBufferingTime = data->timestamp;
        return true;
    }

    if (data != 0)
    {
        m_audioQueue.enqueue(data);
        bufferSizeMs = msInBuffer();
    }


    if (bufferSizeMs >= m_tooFewDataDetected * m_prebufferMs
        || m_audioQueue.size() >= MAX_BUFFER_LEN)
    {
        playCurrentBuffer();
    }

    return true;
}

bool QnAudioStreamDisplay::isPlaying() const
{
    return !m_tooFewDataDetected;
}

void QnAudioStreamDisplay::playCurrentBuffer()
{
    QnMutexLocker lock(&m_audioQueueMutex);
    QnCompressedAudioDataPtr data;
    while (!m_audioQueue.isEmpty())
    {
        m_tooFewDataDetected = false;

        data = m_audioQueue.dequeue();

        //data->dataProvider->setNeedSleep(true); //< need to introduce delay again


        if (data->compressionType == AV_CODEC_ID_NONE)
        {
            NX_ERROR(this, "putdata: unknown codec type...");
            return;
        }
        if (!m_decoders[data->compressionType]->decode(data, m_decodedAudioBuffer))
            return;

        // convert format
        QnCodecAudioFormat audioFormat(data->context);
        if (!m_isConvertMethodInitialized)
        {
            if (m_sound)
            {
                QnMutexLocker lock(&m_guiSync);
                m_sound.reset();
            }

            m_isFormatSupported = initFormatConvertRule(audioFormat);
            m_isConvertMethodInitialized = true;
            if (!m_isFormatSupported)
                return; //< can play audio
        }
        if (m_sampleConvertMethod == SampleConvertMethod::float2Int32)
            audioFormat = QnAudioProcessor::float2int32(m_decodedAudioBuffer, audioFormat);
        else if (m_sampleConvertMethod == SampleConvertMethod::float2Int16)
            audioFormat = QnAudioProcessor::float2int16(m_decodedAudioBuffer, audioFormat);
        else if (m_sampleConvertMethod == SampleConvertMethod::int32ToInt16)
            audioFormat = QnAudioProcessor::int32Toint16(m_decodedAudioBuffer, audioFormat);
        if (audioFormat.channelCount() > 2 && m_downmixing)
            audioFormat = QnAudioProcessor::downmix(m_decodedAudioBuffer, audioFormat);

        //resume(); //< does nothing if resumed already

        // play audio
        if (!m_sound)
        {
            QnMutexLocker lock(&m_guiSync);
            m_sound.reset(nx::audio::AudioDevice::instance()->createSound(audioFormat));
            if (!m_sound)
            {
                // A PC has been seen where: 32-bit format is sometimes supported, sometimes not
                // supported (may be several dlls).
                m_isConvertMethodInitialized = false;
            }
        }

        if (m_sound)
        {
            m_sound->write(
                (const quint8*) m_decodedAudioBuffer.data(), m_decodedAudioBuffer.size());
        }
    }
}

int QnAudioStreamDisplay::msInQueue() const
{
    QnMutexLocker lock(&m_audioQueueMutex);

    if (m_audioQueue.isEmpty())
        return 0;

    //int qsize = m_audioQueue.size();
    //qint64 new_t = m_audioQueue.last()->timestamp;
    //qint64 old_t = m_audioQueue.first()->timestamp;

    qint64 diff =
        m_audioQueue.last()->duration
        + m_audioQueue.last()->timestamp
        - m_audioQueue.first()->timestamp;

    return diff / 1000;
}

void QnAudioStreamDisplay::setForceDownmix(bool value)
{
    m_forceDownmix = value;
    m_isConvertMethodInitialized = false;
}

int QnAudioStreamDisplay::getAudioBufferSize() const
{
    return m_bufferMs;
}
