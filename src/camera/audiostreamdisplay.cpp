#include "audiostreamdisplay.h"
#include "decoders\audio\audio_struct.h"
#include "base\log.h"
#include "decoders\audio\abstractaudiodecoder.h"
#include "base\sleep.h"

// QT_BUFFER_SIZE should be smaller than m_buff_ms/10
static const int QT_BUFFER_SIZE = 200;

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

	int steps = len/6;

	

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
		

		output+=2;
		input+=6;
	
	}
}





CLAudioStreamDisplay::CLAudioStreamDisplay(int buff_ms, int buffAccMs):
m_buff_ms(buff_ms),
m_buffAccMs(buffAccMs),
m_buffAccBytes(0),
m_audioOutput(0),
m_decodedaudio(16, DEFAULT_AUDIO_FRAME_SIZE),
m_audiobuff(0),
m_ringbuff(0),
m_freq_factor(1.0),
m_can_adapt(0),
m_downmixing(false),
m_tooFewSet(false),
m_too_few_data_detected(true)
{
	for (int i = 0; i < CL_VARIOUSE_DECODERS;++i)
		m_decoder[i] = 0;
}

CLAudioStreamDisplay::~CLAudioStreamDisplay()
{
	stopaudio();
	
	for (int i = 0; i < CL_VARIOUSE_DECODERS;++i)
		delete m_decoder[i];

	delete m_ringbuff;
}

int CLAudioStreamDisplay::msInAudio()
{
	int bytesInBuffer = m_audioOutput->bufferSize() - m_audioOutput->bytesFree();
	return ms_from_size(m_audioOutput->format(), bytesInBuffer);
}

int CLAudioStreamDisplay::suspend()
{
	if (!m_audioOutput)
		return 0;

	m_audioOutput->suspend();

	return 0;
}

void CLAudioStreamDisplay::resume()
{
	if (!m_audioOutput)
		return;

	m_audioOutput->resume();
}

void CLAudioStreamDisplay::putdata(CLCompressedAudioData* data)
{
	CLAudioData audio;

	if (data == 0)
		goto fake_audio;

	//return;// 777

	audio.codec = data->compressionType;
	audio.inbuf = (unsigned char*)data->data.data();
	audio.inbuf_len = data->data.size();
	audio.outbuf = &m_decodedaudio;
	audio.outbuf_len = 0;
	audio.format.setChannels(data->channels);
	audio.format.setFrequency(data->freq);



	CLAbstractAudioDecoder* dec = 0;

	if (data->compressionType < 0 || data->compressionType > CL_VARIOUSE_DECODERS - 1)
	{
		cl_log.log("CLAudioStreamDisplay::putdata: unknown codec type...", cl_logERROR);
		return;
	}

	if (m_decoder[data->compressionType] == 0)
	{
		m_decoder[data->compressionType] = CLAudioDecoderFactory::createDecoder(data->compressionType, data->context);
		
	}

	dec = m_decoder[data->compressionType];

	if (dec->decode(audio))
	{

		if (!m_audioOutput)
			recreatedevice(audio.format);

		if (audio.format.channels() > 2 && m_downmixing)
			down_mix(audio);

		if (audio.format.sampleType() == QAudioFormat::Float)	
			float2int(audio);

		if (!m_ringbuff) {
			m_ringbuff = new CLRingBuffer(bytes_from_time(audio.format,m_buff_ms),this);
			m_buffAccBytes = bytes_from_time(audio.format, m_buffAccMs);
		}

		if (!m_audioOutput)
			return;

		if (ms_from_size(audio.format, m_ringbuff->avalable_to_write()) < m_buff_ms / 10) // if ring buffer is full
		{
			//paying too slow; need to speedup
			m_freq_factor += 0.005;

			//audio.format.setFrequency(audio.format.frequency()*m_freq_factor);
			//recreatedevice(audio.format); // recreation does not work very well
			cl_log.log("to many data!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", cl_logALWAYS);
			m_ringbuff->clear();
		}

		if (ms_from_size(audio.format, m_ringbuff->bytesAvailable()) < m_buff_ms / 10) // if ring buffer is almost empty
		{
			//paying too fast; need to slowdown
			m_freq_factor-=0.005;

			//audio.format.setFrequency(audio.format.frequency()*m_freq_factor);
			//recreatedevice(audio.format); //recreation does not work very well
			cl_log.log("to few data in ring buffer!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", cl_logWARNING);
			m_too_few_data_detected = true;

			m_audioOutput->suspend();

			if (m_tooFewSet == false)
			{
				cl_log.log("HERE!", cl_logWARNING);
				m_tooFewSet = true;
			}
		}


		qint64 dataWritten = m_ringbuff->writeData(audio.outbuf->data(), audio.outbuf_len);
		if (dataWritten < audio.outbuf_len)
		{
			cl_log.log("?????????????????????????????????????????????????????????????", cl_logWARNING);
		}

		// cl_log.log("in ring buffer1 ms = ", (int)ms_from_size(audio.format, m_ringbuff->bytesAvailable()), cl_logWARNING);


fake_audio:
		if (m_ringbuff->bytesAvailable() > (m_too_few_data_detected ? m_buffAccBytes  : 0) ) // only if we have more than m_buff_ms/2 ms in data in ring buffer
		{
			m_audioOutput->resume();

			m_too_few_data_detected = false;
			m_tooFewSet = false;

			if (m_audioOutput && m_audioOutput->state() != QAudio::StoppedState) 
			{
				int period = m_audioOutput->periodSize();
				int ring_avalable = m_ringbuff->bytesAvailable();
				int output_avalable = m_audioOutput->bytesFree();

				//cl_log.log("in ring buffer bw ms = ", (int)ms_from_size(audio.format, m_ringbuff->bytesAvailable()), cl_logWARNING);


				int chunks = qMin(ring_avalable, output_avalable) / period;

				if (chunks == 0)
					chunks = 0;

				while (chunks)
				{
					++m_can_adapt; // if few write occurred we can change the frequency if needed

					if (m_decodedaudio.capacity() < period)
						m_decodedaudio.change_capacity(period);


					m_ringbuff->readData(m_decodedaudio.data(), period);
					m_audiobuff->write(m_decodedaudio.data(), period);
					--chunks;
				}

//				cl_log.log("in ring buffer aw ms = ", (int)ms_from_size(audio.format, m_ringbuff->bytesAvailable()), cl_logWARNING);
			}

		}

//		qint64 bytesInBuffer = m_audioOutput->bufferSize() - m_audioOutput->bytesFree();
//		qint64 usInBuffer = ms_from_size(audio.format, bytesInBuffer);
//		cl_log.log("ms in audio buff = ", (int)usInBuffer, cl_logALWAYS);
		//cl_log.log("ms in ring buff = ", (int)ms_from_size(audio.format, m_ringbuff->bytesAvailable()), cl_logALWAYS);
		/**/
	}
}


void CLAudioStreamDisplay::stopaudio()
{
	if (m_audioOutput)
	{
		m_audioOutput->stop();
		delete m_audioOutput;
		m_audioOutput = 0;
	}
}

void CLAudioStreamDisplay::recreatedevice(QAudioFormat format)
{
	stopaudio();

	QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());

	if (!info.isFormatSupported(format)) 
	{
		
		if (format.channels()>2)
		{
			format.setChannels(2);
			if (!info.isFormatSupported(format)) 
			{
				cl_log.log("audio format not supported by backend, cannot play audio.", cl_logERROR);
				return;
			}

			m_downmixing = true;

		}
	}

	m_audioOutput = new QAudioOutput(format, this);

	m_audioOutput->setBufferSize( bytes_from_time(format, QT_BUFFER_SIZE) ); // data for 1sec of audio
	m_audiobuff = m_audioOutput->start();

	m_can_adapt = 0;

}

unsigned int CLAudioStreamDisplay::ms_from_size(const QAudioFormat& format, unsigned long bytes)
{
	return (qint64)(1000) * bytes/ ( format.channels() * format.sampleSize() / 8 ) / format.frequency();
}

unsigned int CLAudioStreamDisplay::bytes_from_time(const QAudioFormat& format, unsigned long ms)
{
	return ( (qint64)format.channels() * format.sampleSize() / 8 ) * format.frequency() * ms / 1000;
}

void CLAudioStreamDisplay::down_mix(CLAudioData& audio)
{
	if (audio.format.channels()==6)
	{
		down_mix_6_2((short*)(audio.outbuf->data()), audio.outbuf_len);
		audio.outbuf_len/=3;
		audio.format.setChannels(2);
	}
}

void CLAudioStreamDisplay::float2int(CLAudioData& audio)
{
	Q_ASSERT(sizeof(float)==4); // not sure about sizeof(float) in 64 bit version

	qint32* p = (qint32*)(audio.outbuf->data());
	int len = audio.outbuf_len/4;

	for (int i = 0; i < len; ++i)
	{
		float f = *((float*)p); 
		*p = (qint32)(f*(1<<31));
		++p;
	}

	audio.format.setSampleType(QAudioFormat::SignedInt);
	
}

bool CLAudioStreamDisplay::is_initialized() const
{
	return m_ringbuff != 0;
}

void CLAudioStreamDisplay::clear()
{
	if (m_ringbuff) 
		m_ringbuff->clear();

	if (m_audioOutput)
	{
		m_audioOutput->reset();
		m_audioOutput->start();
	}
}
