#include "audiostreamdisplay.h"
#include "decoders/audio/audio_struct.h"
#include "base/log.h"
#include "decoders/audio/abstractaudiodecoder.h"
#include "base/sleep.h"
#include "streamreader/streamreader.h"
#include "audio_device.h"

#define DEFAULT_AUDIO_FRAME_SIZE (AVCODEC_MAX_AUDIO_FRAME_SIZE*2)
int  MAX_AUDIO_FRAME_SIZE = DEFAULT_AUDIO_FRAME_SIZE*5;

static short clip_short(int v)
{
	if (v < -32768)
        v = -32768;
    else if (v > 32767)
        v = 32767;

    return (short) v;
}

static void down_mix_6_2(short *data, int len) 
{
	short* output = data;
	short* input = data;

	int steps = len / 6;

	for(int i = 0; i < steps; ++i) 
	{
	    /* 5.1 to stereo. l, c, r, ls, rs, sw */
		int fl,fr,c,rl,rr,lfe;

		fl = input[0];
		c = input[2];
		fr = input[1];
		rl = input[3];
		rr = input[4];
		//lfe = input[5]; //Postings on the Doom9 say that Dolby specifically says the LFE (.1) channel should usually be ignored during downmixing to Dolby ProLogic II, with quotes from official Dolby documentation.

		output[0] = clip_short(fl + (0.5 * rl) + (0.7 * c));
		output[1] = clip_short(fr + (0.5 * rr) + (0.7 * c));

		output += 2;
		input += 6;
	}
}

CLAudioStreamDisplay::CLAudioStreamDisplay(int bufferMs)
    : m_bufferMs(bufferMs),
      m_decodedaudio(16, DEFAULT_AUDIO_FRAME_SIZE),
      m_downmixing(false),
      m_tooFewDataDetected(true),
      m_audioDevice(0)
{
}

CLAudioStreamDisplay::~CLAudioStreamDisplay()
{
	delete m_audioDevice;

    foreach(CLAbstractAudioDecoder* decoder, m_decoder)
    {
        delete decoder;
    }
}

int CLAudioStreamDisplay::msInBuffer() const
{
    return msInQueue() + (m_audioDevice ? m_audioDevice->msInBuffer() : 0);
}

void CLAudioStreamDisplay::suspend()
{
	if (!m_audioDevice)
		return;

	m_audioDevice->suspend();
}

void CLAudioStreamDisplay::resume()
{
	if (!m_audioDevice)
		return;

	m_audioDevice->resume();
}

bool CLAudioStreamDisplay::isBuffering() const
{
	return m_tooFewDataDetected;
}

void CLAudioStreamDisplay::clearDeviceBuffer()
{
    if (m_audioDevice)
        m_audioDevice->clearAudioBuff();
}

void CLAudioStreamDisplay::clearAudioBuffer()
{
    while (!m_audioQueue.isEmpty())
    {
        m_audioQueue.dequeue()->releaseRef();
    }

    clearDeviceBuffer();
}

void CLAudioStreamDisplay::enqueueData(CLCompressedAudioData* data)
{
    data->addRef();
    m_audioQueue.enqueue(data);
    m_audioQueue.dequeue()->releaseRef();
}

void CLAudioStreamDisplay::putData(CLCompressedAudioData* data)
{
	if (data == 0 && !m_audioDevice) // do not need to check audio device in case of data=0 and no audio device
        return;

    // some times distance between audio packets in file is very large ( may be more than audio_device buffer );
    // audio_device buffer is small, and we need to put data from packets audio device. to do it we call this function with 0 pinter

    //cl_log.log("AUDIO_QUUE = ", m_audioQueue.size(), cl_logALWAYS);

    if (data!=0)
	{
        data->addRef();

        if (m_audioQueue.size()<1000)
            m_audioQueue.enqueue(data);
        else
        {
            data->releaseRef();
            return;
        }

        if (msInQueue() > m_bufferMs)
        {
            cl_log.log("to many data in audio queue!!!!", cl_logDEBUG1);
            clearAudioBuffer();
            return;
        }

        if (msInQueue() < m_bufferMs / 10)
        {
            //paying too fast; need to slowdown
            cl_log.log("too few data in audio queue!!!!", cl_logDEBUG1);
            m_tooFewDataDetected = true;
            suspend();
            data->dataProvider->setNeedSleep(false); //lets reader run without delays;
            return;
        }
	}

    if (m_audioQueue.empty()) // possible if incoming data = 0
        return;

    if (m_audioDevice && !m_audioDevice->wantMoreData())
        return;

    if (msInQueue() > m_tooFewDataDetected*playAfterMs() )
    {
        m_tooFewDataDetected = false;
        resume(); // does nothing if resumed already

        data = m_audioQueue.dequeue();

        data->dataProvider->setNeedSleep(true); // need to introduce delay again

        CLAudioData audio;
        audio.codec = data->compressionType;
        audio.inbuf = (unsigned char*)data->data.data();
        audio.inbuf_len = data->data.size();
        audio.outbuf = &m_decodedaudio;
        audio.outbuf_len = 0;
        audio.format.setChannels(data->channels);
        audio.format.setFrequency(data->freq);

        CLAbstractAudioDecoder* dec = 0;

        if (data->compressionType == CODEC_ID_NONE)
        {
            cl_log.log("CLAudioStreamDisplay::putdata: unknown codec type...", cl_logERROR);
            data->releaseRef();
            return;
        }

        if (m_decoder[data->compressionType] == 0)
        {
            m_decoder[data->compressionType] = CLAudioDecoderFactory::createDecoder(data->compressionType, data->context);

        }

        dec = m_decoder[data->compressionType];

        bool decoded = dec->decode(audio);
        data->releaseRef();

        if (!decoded || audio.outbuf_len == 0)
            return;

        if (!m_audioDevice)
            m_audioDevice = new CLAudioDevice(audio.format);

        if (m_audioDevice->convertingFloat())
        {
            float2int(audio);
            audio.outbuf_len /= 2;
        }

        if (audio.format.channels() > 2 && m_audioDevice->downmixing())
            downmix(audio);


        m_audioDevice->write(audio.outbuf->data(), audio.outbuf_len);
    }

//		qint64 bytesInBuffer = m_audioOutput->bufferSize() - m_audioOutput->bytesFree();
//		qint64 usInBuffer = ms_from_size(audio.format, bytesInBuffer);
//		cl_log.log("ms in audio buff = ", (int)usInBuffer, cl_logALWAYS);
		//cl_log.log("ms in ring buff = ", (int)ms_from_size(m_audio.format, m_ringbuff->bytesAvailable()), cl_logALWAYS);
		/**/
}

//=======================================================================
void CLAudioStreamDisplay::downmix(CLAudioData& audio)
{
	if (audio.format.channels() == 6)
	{
		down_mix_6_2((short*)(audio.outbuf->data()), audio.outbuf_len);
		audio.outbuf_len /= 3;
		audio.format.setChannels(2);
	}
}

void CLAudioStreamDisplay::float2int(CLAudioData& audio)
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
