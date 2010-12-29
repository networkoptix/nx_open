#include "audiostreamdisplay.h"
#include "decoders\audio\audio_struct.h"
#include "base\log.h"
#include "decoders\audio\abstractaudiodecoder.h"
#include <QAudioOutput>

#define MAX_AUDIO_FRAME_SIZE 192000



CLAudioStreamDisplay::CLAudioStreamDisplay(int buff_ms):
m_buff_ms(buff_ms),
m_audioOutput(0),
m_decodedaudio(16, MAX_AUDIO_FRAME_SIZE),
m_audiobuff(0),
m_ringbuff(0),
m_freq_factor(1.0),
m_can_adapt(0)
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

void CLAudioStreamDisplay::putdata(CLCompressedAudioData* data)
{
	CLAudioData audio;

	audio.codec = data->compressionType;
	audio.inbuf = (unsigned char*)data->data.data();
	audio.inbuf_len = data->data.size();
	audio.outbuf = m_decodedaudio;
	audio.outbuf_len = 0;
	audio.format.setChannels(data->channels);
	audio.format.setFrequency(data->freq);



	CLAbstractAudioDecoder* dec = 0;

	if (data->compressionType<0 || data->compressionType>CL_VARIOUSE_DECODERS-1)
	{
		cl_log.log("CLAudioStreamDisplay::putdata: unknown codec type...", cl_logERROR);
		return;
	}

	if (m_decoder[data->compressionType]==0)
	{
		m_decoder[data->compressionType] = CLAudioDecoderFactory::createDecoder(data->compressionType);
		
	}

	dec = m_decoder[data->compressionType];

	

	if (dec->decode(audio))
	{
		
		if (!m_ringbuff)
			m_ringbuff = new CLRingBuffer(bytes_from_time(audio.format,m_buff_ms*1.5),this);
		

		if (!m_audioOutput)
			recreatedevice(audio.format);

		if (!m_audioOutput)
			return;


		if (m_can_adapt>30 && ms_from_size(audio.format, m_ringbuff->bytesAvailable()) > m_buff_ms) // if ring buffer is full
		{
			//paying too slow; need to speedup
			m_freq_factor+=0.005;
			audio.format.setFrequency(audio.format.frequency()*m_freq_factor);
			//recreatedevice(audio.format); // recreation does not work very well
			m_ringbuff->claer();
		}

		if (m_can_adapt>30 && ms_from_size(audio.format, m_audioOutput->bytesFree()) > 0.8*m_buff_ms) // if audiobuffer is almost empty
		{
			//paying too fast; need to slowdown
			m_freq_factor-=0.005;
			audio.format.setFrequency(audio.format.frequency()*m_freq_factor);
			//recreatedevice(audio.format); //recreation does not work very well
		}

			

		m_ringbuff->writeData((char*)audio.outbuf, audio.outbuf_len);

		if (ms_from_size(audio.format, m_ringbuff->bytesAvailable()) > 0) // only if we have more than 100 ms in data in ring buffer
		{

			if (m_audioOutput && m_audioOutput->state() != QAudio::StoppedState) 
			{

				int period = m_audioOutput->periodSize();
				int ring_avalable = m_ringbuff->bytesAvailable();
				int output_avalable = m_audioOutput->bytesFree();


				int chunks = qMin( ring_avalable, output_avalable)/period;
				while (chunks) 
				{
					++m_can_adapt; // if few write occurred we can change the frequency if needed

					m_ringbuff->readData(m_decodedaudio, period);
					m_audiobuff->write(m_decodedaudio, period);
					--chunks;
				}
			}

		}


		

		qint64 bytesInBuffer = m_audioOutput->bufferSize() - m_audioOutput->bytesFree();
		qint64 usInBuffer = ms_from_size(audio.format, bytesInBuffer);
		//cl_log.log("ms in audio buff = ", (int)usInBuffer, cl_logALWAYS);
		//cl_log.log("ms in ring buff = ", (int)ms_from_size(audio.format, m_ringbuff->bytesAvailable()), cl_logALWAYS);

		//m_audiobuff->write((char*)audio.outbuf, audio.outbuf_len);
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
		cl_log.log("audio format not supported by backend, cannot play audio.", cl_logERROR);
		return;
	}

	m_audioOutput = new QAudioOutput(format, this);
	m_audioOutput->setBufferSize( bytes_from_time(format, m_buff_ms) ); // data for 1sec of audio
	m_audiobuff = m_audioOutput->start();

	m_can_adapt = 0;

}


unsigned int CLAudioStreamDisplay::ms_from_size(const QAudioFormat& format, unsigned long bytes)
{
	return (qint64)(1000) * bytes/ ( format.channels() * format.sampleSize() / 8 ) / format.frequency();
}

unsigned int CLAudioStreamDisplay::bytes_from_time(const QAudioFormat& format, unsigned long ms)
{
	return ( format.channels() * format.sampleSize() / 8 ) * format.frequency() * ms / 1000;
}