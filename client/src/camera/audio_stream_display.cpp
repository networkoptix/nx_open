#include "audio_stream_display.h"

#include <utils/common/log.h>
#include <utils/media/audio_processor.h>

#include "decoders/audio/audio_struct.h"
#include "decoders/audio/abstractaudiodecoder.h"

#include "client/client_settings.h"


#define DEFAULT_AUDIO_FRAME_SIZE (AVCODEC_MAX_AUDIO_FRAME_SIZE*2)
int  MAX_AUDIO_FRAME_SIZE = DEFAULT_AUDIO_FRAME_SIZE*5;

QnAudioStreamDisplay::QnAudioStreamDisplay(int bufferMs, int prebufferMs):
    m_bufferMs(bufferMs),
    m_prebufferMs(prebufferMs),
    m_tooFewDataDetected(true),
    m_isFormatSupported(true),
    m_audioSound(0),
    m_downmixing(false),
    m_forceDownmix(qnSettings->isAudioDownmixed()),
    m_sampleConvertMethod(SampleConvert_None),
    m_isConvertMethodInitialized(false),
    m_decodedAudioBuffer(CL_MEDIA_ALIGNMENT, AVCODEC_MAX_AUDIO_FRAME_SIZE),
    m_startBufferingTime(AV_NOPTS_VALUE)
{}

QnAudioStreamDisplay::~QnAudioStreamDisplay()
{
    if (m_audioSound)
        QtvAudioDevice::instance()->removeSound(m_audioSound);

    foreach(CLAbstractAudioDecoder* decoder, m_decoder)
    {
        delete decoder;
    }
}

int QnAudioStreamDisplay::msInBuffer() const
{
    int internalBufferSize = (m_audioSound ? m_audioSound->playTimeElapsed()/1000.0 : 0);

    //cl_log.log("internalBufferSize = ", internalBufferSize, cl_logALWAYS);
    //cl_log.log("compressedBufferSize = ", msInQueue(), cl_logALWAYS);
    //cl_log.log("tptalaudio = ", msInQueue() + internalBufferSize, cl_logALWAYS);

    int rez = msInQueue() + internalBufferSize;

    return rez;
}

void QnAudioStreamDisplay::suspend()
{
    QMutexLocker lock(&m_guiSync);
    if (m_audioSound) {
        m_tooFewDataDetected = true;
        m_audioSound->suspend();
    }
}

void QnAudioStreamDisplay::resume()
{
    QMutexLocker lock(&m_guiSync);
    if (m_audioSound)
        m_audioSound->resume();
}

qint64 QnAudioStreamDisplay::startBufferingTime() const
{
    return m_tooFewDataDetected ? m_startBufferingTime : AV_NOPTS_VALUE;
}

bool QnAudioStreamDisplay::isFormatSupported() const
{
    if (!m_audioSound)
        return true; // it's not constructed yet

    //return m_audioSound->isFormatSupported();
    return m_isFormatSupported;
}

void QnAudioStreamDisplay::clearDeviceBuffer()
{
    if (m_audioSound)
        m_audioSound->clear();
}

void QnAudioStreamDisplay::clearAudioBuffer()
{
    while (!m_audioQueue.isEmpty())
    {
        m_audioQueue.dequeue();
    }

    clearDeviceBuffer();
    m_tooFewDataDetected = true;
}

void QnAudioStreamDisplay::enqueueData(QnCompressedAudioDataPtr data, qint64 minTime)
{
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
    else {
        m_downmixing = false;
    }

    if (QtvSound::isFormatSupported(format))
        return true;

    if (format.sampleType() == QnAudioFormat::Float)
    {
        format.setSampleType(QnAudioFormat::SignedInt);
        if (QtvSound::isFormatSupported(format))
        {
            m_sampleConvertMethod = SampleConvert_Float2Int32;
            return true;
        }
        format.setSampleSize(16);
        if (QtvSound::isFormatSupported(format))
        {
            m_sampleConvertMethod = SampleConvert_Float2Int16;
            return true;
        }
    }
    else if (format.sampleSize() == 32)
    {
        format.setSampleSize(16);
        if (QtvSound::isFormatSupported(format))
        {
            m_sampleConvertMethod = SampleConvert_Int32ToInt16;
            return true;
        }
    }

    format.setChannelCount(2);
    if (QtvSound::isFormatSupported(format))
    {
        m_downmixing = true;
        return true;
    }
    return false; // conversion rule not found
}

void QnAudioStreamDisplay::putData(QnCompressedAudioDataPtr data, qint64 minTime)
{
    static const int MAX_BUFFER_LEN = 3000;
    if (data == 0 && !m_audioSound) // do not need to check audio device in case of data=0 and no audio device
        return;

    // some times distance between audio packets in file is very large ( may be more than audio_device buffer );
    // audio_device buffer is small, and we need to put data from packets audio device. to do it we call this function with 0 pinter

    //cl_log.log("AUDIO_QUUE = ", m_audioQueue.size(), cl_logALWAYS);

    int bufferSize = msInBuffer();
    if (bufferSize < m_bufferMs / 10)
    {
        m_tooFewDataDetected = true;
        m_startBufferingTime = data->timestamp - bufferSize;
    }

    if (m_tooFewDataDetected && data && data->timestamp < minTime)
    {
        clearAudioBuffer();
        m_startBufferingTime = data->timestamp;
        return;
    }

    if (data!=0)
    {
        m_audioQueue.enqueue(data);
        bufferSize = msInBuffer();
    }


    if (bufferSize >= m_tooFewDataDetected * m_prebufferMs || m_audioQueue.size() >= MAX_BUFFER_LEN)
    {
        playCurrentBuffer();
    }
    //qint64 bytesInBuffer = m_audioOutput->bufferSize() - m_audioOutput->bytesFree();
    //qint64 usInBuffer = ms_from_size(audio.format, bytesInBuffer);
    //cl_log.log("ms in audio buff = ", (int)usInBuffer, cl_logALWAYS);
    //cl_log.log("ms in ring buff = ", (int)ms_from_size(m_audio.format, m_ringbuff->bytesAvailable()), cl_logALWAYS);
    /**/
}

void QnAudioStreamDisplay::playCurrentBuffer()
{
    QnCompressedAudioDataPtr data;
    while (!m_audioQueue.isEmpty() )
    {
        m_tooFewDataDetected = false;

        data = m_audioQueue.dequeue();

        //data->dataProvider->setNeedSleep(true); // need to introduce delay again

        /*
        CLAudioData audio;
        audio.codec = data->compressionType;
        audio.inbuf = (unsigned char*)data->data.data();
        audio.inbuf_len = data->data.size();
        audio.outbuf = &m_decodedaudio;
        audio.outbuf_len = 0;
        audio.format = data->format;
        */

        if (data->compressionType == CODEC_ID_NONE)
        {
            cl_log.log(QLatin1String("QnAudioStreamDisplay::putdata: unknown codec type..."), cl_logERROR);
            return;
        }

        if (m_decoder[data->compressionType] == 0)
        {
            m_decoder[data->compressionType] = CLAudioDecoderFactory::createDecoder(data);

        }

        if (!m_decoder[data->compressionType]->decode(data, m_decodedAudioBuffer))
            return;

        //  convert format
        QnCodecAudioFormat audioFormat;
        audioFormat.fromAvStream(data->context->ctx());
        if (!m_isConvertMethodInitialized)
        {
            if (m_audioSound)
            {
                QMutexLocker lock(&m_guiSync);
                QtvAudioDevice::instance()->removeSound(m_audioSound);
                m_audioSound = 0;
            }

            m_isFormatSupported = initFormatConvertRule(audioFormat);
            m_isConvertMethodInitialized = true;
            if (!m_isFormatSupported)
                return; // can play audio
        }
        if (m_sampleConvertMethod == SampleConvert_Float2Int32)
            audioFormat = QnAudioProcessor::float2int32(m_decodedAudioBuffer, audioFormat);
        else if (m_sampleConvertMethod == SampleConvert_Float2Int16)
            audioFormat = QnAudioProcessor::float2int16(m_decodedAudioBuffer, audioFormat);
        else if (m_sampleConvertMethod == SampleConvert_Int32ToInt16)
            audioFormat = QnAudioProcessor::int32Toint16(m_decodedAudioBuffer, audioFormat);
        if (audioFormat.channelCount() > 2 && m_downmixing)
            audioFormat = QnAudioProcessor::downmix(m_decodedAudioBuffer, audioFormat);

        //resume(); // does nothing if resumed already

        // play audio
        if (!m_audioSound) 
        {
            QMutexLocker lock(&m_guiSync);
            m_audioSound = QtvAudioDevice::instance()->addSound(audioFormat);
            if (!m_audioSound)
                m_isConvertMethodInitialized = false; // I have found PC where: 32-bit format sometime supported, sometimes not supported (may be several dll)
        }

        if (m_audioSound)
            m_audioSound->play((const quint8*) m_decodedAudioBuffer.data(), m_decodedAudioBuffer.size());
    }
}

int QnAudioStreamDisplay::msInQueue() const
{
    if (m_audioQueue.isEmpty())
        return 0;

    //int qsize = m_audioQueue.size();
    //qint64 new_t = m_audioQueue.last()->timestamp;
    //qint64 old_t = m_audioQueue.first()->timestamp;

    qint64 diff = m_audioQueue.last()->duration + m_audioQueue.last()->timestamp  - m_audioQueue.first()->timestamp;

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
