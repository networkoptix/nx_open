#include "audiostreamdisplay.h"
#include "decoders/audio/audio_struct.h"
#include "decoders/audio/abstractaudiodecoder.h"

#include "utils/settings.h"

#define DEFAULT_AUDIO_FRAME_SIZE (AVCODEC_MAX_AUDIO_FRAME_SIZE*2)
int  MAX_AUDIO_FRAME_SIZE = DEFAULT_AUDIO_FRAME_SIZE*5;

static inline short clip_short(int v)
{
    if (v < -32768)
        return -32768;
    else if (v > 32767)
        return 32767;
    else
        return (short) v;
}

template <class T>
static void down_mix_to_stereo(T *data, int channels, int len)
{
    T* output = data;
    T* input = data;

    int steps = len / 6 / sizeof(T);

    for(int i = 0; i < steps; ++i)
    {
        /* 5.1 to stereo. l, c, r, ls, rs, sw */
        int fl,fr,c,rl,rr,lfe;

        fl = input[0];
        fr = input[1];
        if (channels >= 6)
        {
            c =  input[2];
            lfe = input[3]; //Postings on the Doom9 say that Dolby specifically says the LFE (.1) channel should usually be ignored during downmixing to Dolby ProLogic II, with quotes from official Dolby documentation.
            rl = input[4];
            rr = input[5];

            output[0] = clip_short(fl + (0.5 * rl) + (0.7 * c));
            output[1] = clip_short(fr + (0.5 * rr) + (0.7 * c));
        }
        else
        {
            output[0] = fl;
            output[1] = fr;
        }
        output += 2;
        input += channels;
    }
}

CLAudioStreamDisplay::CLAudioStreamDisplay(int bufferMs) :
    m_bufferMs(bufferMs),
    m_decodedaudio(16, DEFAULT_AUDIO_FRAME_SIZE),
    m_tooFewDataDetected(true),
    m_isFormatSupported(true),
    m_audioSound(0),
    m_downmixing(false),
    m_forceDownmix(qnSettings->downmixAudio()),
    m_sampleConvertMethod(SampleConvert_None),
    m_isConvertMethodInitialized(false)
{
}

CLAudioStreamDisplay::~CLAudioStreamDisplay()
{
    if (m_audioSound)
        QtvAudioDevice::instance()->removeSound(m_audioSound);

    foreach(CLAbstractAudioDecoder* decoder, m_decoder)
    {
        delete decoder;
    }
}

int CLAudioStreamDisplay::msInBuffer() const
{
    int internalBufferSize = (m_audioSound ? m_audioSound->playTimeElapsed()/1000.0 : 0);

    //cl_log.log("internalBufferSize = ", internalBufferSize, cl_logALWAYS);
    //cl_log.log("compressedBufferSize = ", msInQueue(), cl_logALWAYS);
    //cl_log.log("tptalaudio = ", msInQueue() + internalBufferSize, cl_logALWAYS);

    return msInQueue() + internalBufferSize;
}

void CLAudioStreamDisplay::suspend()
{
    if (!m_audioSound)
        return;
    m_tooFewDataDetected = true;
    m_audioSound->suspend();
}

void CLAudioStreamDisplay::resume()
{
    if (!m_audioSound)
        return;

    m_audioSound->resume();
}

bool CLAudioStreamDisplay::isBuffering() const
{
    return m_tooFewDataDetected;
}

bool CLAudioStreamDisplay::isFormatSupported() const
{
    if (!m_audioSound)
        return true; // it's not constructed yet

    //return m_audioSound->isFormatSupported();
    return m_isFormatSupported;
}

void CLAudioStreamDisplay::clearDeviceBuffer()
{
    if (m_audioSound)
        m_audioSound->clear();
}

void CLAudioStreamDisplay::clearAudioBuffer()
{
    while (!m_audioQueue.isEmpty())
    {
        m_audioQueue.dequeue();
    }

    clearDeviceBuffer();
    m_tooFewDataDetected = true;
}

void CLAudioStreamDisplay::enqueueData(QnCompressedAudioDataPtr data, qint64 minTime)
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

bool CLAudioStreamDisplay::initFormatConvertRule(QnAudioFormat format)
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

void CLAudioStreamDisplay::putData(QnCompressedAudioDataPtr data, qint64 minTime)
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
    }

    if (m_tooFewDataDetected && data && data->timestamp < minTime)
    {
        clearAudioBuffer();
        return;
    }

    if (data!=0)
    {
        m_audioQueue.enqueue(data);
    }


    if (bufferSize >= m_tooFewDataDetected*playAfterMs() || m_audioQueue.size() >= MAX_BUFFER_LEN)
    {
        playCurrentBuffer();
    }
//		qint64 bytesInBuffer = m_audioOutput->bufferSize() - m_audioOutput->bytesFree();
//		qint64 usInBuffer = ms_from_size(audio.format, bytesInBuffer);
//		cl_log.log("ms in audio buff = ", (int)usInBuffer, cl_logALWAYS);
        //cl_log.log("ms in ring buff = ", (int)ms_from_size(m_audio.format, m_ringbuff->bytesAvailable()), cl_logALWAYS);
        /**/
}

void CLAudioStreamDisplay::playCurrentBuffer()
{
    QnCompressedAudioDataPtr data;
    while (!m_audioQueue.isEmpty() )
    {
        m_tooFewDataDetected = false;

        data = m_audioQueue.dequeue();

        //data->dataProvider->setNeedSleep(true); // need to introduce delay again

        CLAudioData audio;
        audio.codec = data->compressionType;
        audio.inbuf = (unsigned char*)data->data.data();
        audio.inbuf_len = data->data.size();
        audio.outbuf = &m_decodedaudio;
        audio.outbuf_len = 0;
        audio.format = data->format;

        if (data->compressionType == CODEC_ID_NONE)
        {
            cl_log.log(QLatin1String("CLAudioStreamDisplay::putdata: unknown codec type..."), cl_logERROR);
            return;
        }

        if (m_decoder[data->compressionType] == 0)
        {
            m_decoder[data->compressionType] = CLAudioDecoderFactory::createDecoder(data);

        }

        bool decoded = m_decoder[data->compressionType]->decode(audio);

        if (!decoded || audio.outbuf_len == 0)
            return;

        //  convert format
        if (!m_isConvertMethodInitialized)
        {
            if (m_audioSound)
            {
                QtvAudioDevice::instance()->removeSound(m_audioSound);
                m_audioSound = 0;
            }

            m_isFormatSupported = initFormatConvertRule(audio.format);
            m_isConvertMethodInitialized = true;
            if (!m_isFormatSupported)
                return; // can play audio
        }
        if (m_sampleConvertMethod == SampleConvert_Float2Int32)
            float2int32(audio);
        else if (m_sampleConvertMethod == SampleConvert_Float2Int16)
            float2int16(audio);
        else if (m_sampleConvertMethod == SampleConvert_Int32ToInt16)
            int32Toint16(audio);
        if (audio.format.channels() > 2 && m_downmixing)
            downmix(audio);

        //resume(); // does nothing if resumed already

        // play audio
        if (!m_audioSound) {
            m_audioSound = QtvAudioDevice::instance()->addSound(audio.format);
            if (!m_audioSound)
                m_isConvertMethodInitialized = false; // I have found PC where: 32-bit format sometime supported, sometimes not supported (may be several dll)
        }

        if (m_audioSound)
            m_audioSound->play((const quint8*) audio.outbuf->data(), audio.outbuf_len);
    }
}

//=======================================================================
void CLAudioStreamDisplay::downmix(CLAudioData& audio)
{
    if (audio.format.channels() > 2)
    {
        if (audio.format.sampleSize() == 8)
            down_mix_to_stereo<qint8>((qint8*)(audio.outbuf->data()), audio.format.channelCount(), audio.outbuf_len);
        else if (audio.format.sampleSize() == 16)
            down_mix_to_stereo<qint16>((qint16*)(audio.outbuf->data()), audio.format.channelCount(), audio.outbuf_len);
        else if (audio.format.sampleSize() == 32)
            down_mix_to_stereo<qint32>((qint32*)(audio.outbuf->data()), audio.format.channelCount(), audio.outbuf_len);
        else
        {
            Q_ASSERT_X(1 == 0, Q_FUNC_INFO + __LINE__, "invalid sample size");
        }
        audio.outbuf_len /= audio.format.channelCount();
        audio.outbuf_len *= 2;
        audio.format.setChannels(2);
    }
}

void CLAudioStreamDisplay::float2int16(CLAudioData& audio)
{
    Q_ASSERT(sizeof(float) == 4); // not sure about sizeof(float) in 64 bit version

    qint32* inP = (qint32*)(audio.outbuf->data());
    qint16* outP = (qint16*)inP;
    int len = audio.outbuf_len/4;

    for (int i = 0; i < len; ++i)
    {
        float f = *((float*)inP);
        *outP = (qint16)(f * (1 << 15));
        ++inP;
        ++outP;
    }
    audio.outbuf_len /= 2;
    audio.format.setSampleSize(16);
    audio.format.setSampleType(QnAudioFormat::SignedInt);
}

void CLAudioStreamDisplay::int32Toint16(CLAudioData& audio)
{
    Q_ASSERT(sizeof(float) == 4); // not sure about sizeof(float) in 64 bit version

    qint32* inP = (qint32*)(audio.outbuf->data());
    qint16* outP = (qint16*)inP;
    int len = audio.outbuf_len/4;

    for (int i = 0; i < len; ++i)
    {
        *outP = *inP >> 16;
        ++inP;
        ++outP;
    }
    audio.outbuf_len /= 2;
    audio.format.setSampleSize(16);
    audio.format.setSampleType(QnAudioFormat::SignedInt);
}

void CLAudioStreamDisplay::float2int32(CLAudioData& audio)
{
    Q_ASSERT(sizeof(float) == 4); // not sure about sizeof(float) in 64 bit version

    qint32* inP = (qint32*)(audio.outbuf->data());
    int len = audio.outbuf_len/4;
    for (int i = 0; i < len; ++i)
    {
        float f = *((float*)inP);
        *inP = (qint32) (f * INT_MAX);
        ++inP;
    }
    audio.format.setSampleType(QnAudioFormat::SignedInt);
}

int CLAudioStreamDisplay::playAfterMs() const
{
    return m_bufferMs / 2;
}

int CLAudioStreamDisplay::msInQueue() const
{
    if (m_audioQueue.isEmpty())
        return 0;

    //int qsize = m_audioQueue.size();
    //qint64 new_t = m_audioQueue.last()->timestamp;
    //qint64 old_t = m_audioQueue.first()->timestamp;

    qint64 diff = m_audioQueue.last()->duration + m_audioQueue.last()->timestamp  - m_audioQueue.first()->timestamp;

    return diff / 1000;
}

void CLAudioStreamDisplay::setForceDownmix(bool value)
{
    m_forceDownmix = value;
    m_isConvertMethodInitialized = false;
}

int CLAudioStreamDisplay::getAudioBufferSize() const
{
    return m_bufferMs;
}