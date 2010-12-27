#include "audiostreamdisplay.h"
#include "decoders\audio\audio_struct.h"
#include "base\log.h"
#include "decoders\audio\abstractaudiodecoder.h"
#include <QAudioOutput>

#define MAX_AUDIO_FRAME_SIZE 192000




CLRingBuffer::CLRingBuffer( int capacity, QObject* parent):
QIODevice(parent),
m_capacity(capacity),
m_mtx(QMutex::Recursive)
{
	m_buff = new char[m_capacity];
	m_pw = m_buff;
	m_pr = m_buff;

	
	open(QIODevice::ReadWrite);
}

CLRingBuffer::~CLRingBuffer()
{
	delete[] m_buff;
}

qint64 CLRingBuffer::bytesAvailable() const
{
	QMutexLocker mutex(&m_mtx);
	return (m_pr <= m_pw) ? m_pw - m_pr : m_capacity - (m_pr - m_pw);
}

qint64 CLRingBuffer::avalable_to_write() const
{
	QMutexLocker mutex(&m_mtx);
	return (m_pw >= m_pr) ? m_capacity - (m_pw - m_pr) : m_pr - m_pw;
}


qint64 CLRingBuffer::readData(char *data, qint64 maxlen)
{
	QMutexLocker mutex(&m_mtx);

	qint64 can_read = bytesAvailable();

	qint64 to_read = qMin(can_read, maxlen);


	if (m_pr + to_read<= m_buff + m_capacity)
	{
		memcpy(data, m_pr, to_read);
		m_pr+=to_read;

	}
	else
	{
		int first_read = m_buff + m_capacity - m_pr;
		memcpy(data, m_pr, first_read);

		int second_read = to_read - first_read;
		memcpy(data + first_read, m_buff, second_read);
		m_pr = m_buff + second_read;


	}

	return to_read;


}

qint64 CLRingBuffer::writeData(const char *data, qint64 len)
{
	QMutexLocker mutex(&m_mtx);

	qint64 can_write = avalable_to_write();

	
	qint64 to_write = qMin(len, can_write);

	if (m_pw + to_write <= m_buff + m_capacity)
	{
		memcpy(m_pw, data, to_write);
		m_pw+=to_write;

	}
	else
	{
		int first_write = m_buff + m_capacity - m_pw;
		memcpy(m_pw, data, first_write);

		int second_write = to_write - first_write;
		memcpy(m_buff, data + first_write, second_write);
		m_pw = m_buff + second_write;


	}

	return to_write;

}





CLAudioStreamDisplay::CLAudioStreamDisplay(int buff_ms):
m_recommended_buff_ms(buff_ms),
m_audioOutput(0),
m_ringbuff(200*1024, this)
{
	for (int i = 0; i < CL_VARIOUSE_DECODERS;++i)
		m_decoder[i] = 0;

	m_decodedaudio = new unsigned char[MAX_AUDIO_FRAME_SIZE];

	

}

CLAudioStreamDisplay::~CLAudioStreamDisplay()
{
	stopaudio();
	
	for (int i = 0; i < CL_VARIOUSE_DECODERS;++i)
		delete m_decoder[i];


	delete[] m_decodedaudio;


}

void CLAudioStreamDisplay::putdata(CLCompressedAudioData* data)
{
	CLAudioData audio;

	audio.codec = data->compressionType;
	audio.inbuf = (unsigned char*)data->data.data();
	audio.inbuf_len = data->data.size();
	audio.outbuf = m_decodedaudio;
	audio.outbuf_len = 0;

	if ((data->data.size()%2)!=0)
	{
		
	}


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

		//m_ringbuff.writeData((char*)audio.outbuf, audio.outbuf_len);

		if (!m_audioOutput)
		{
			QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());

			if (!info.isFormatSupported(audio.format)) 
			{
				cl_log.log("audio format not supported by backend, cannot play audio.", cl_logERROR);
				return;
			}

			m_audioOutput = new QAudioOutput(audio.format, this);
			connect(m_audioOutput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(stateChanged(QAudio::State)));

			//m_audioOutput->setBufferSize(MAX_AUDIO_FRAME_SIZE);
			//m_audioOutput->start(m_ringbuff);

			
			m_audiobuff = m_audioOutput->start();

		}

		

		qint64 bytesInBuffer = m_audioOutput->bufferSize() - m_audioOutput->bytesFree();
		qint64 usInBuffer = (qint64)(1000000) * bytesInBuffer / ( audio.format.channels() * audio.format.sampleSize() / 8 ) / audio.format.frequency();
		//cl_log.log("ms1 in buff = ", (int)usInBuffer/1000, cl_logALWAYS);

		m_audiobuff->write((char*)audio.outbuf, audio.outbuf_len);

		bytesInBuffer = m_audioOutput->bufferSize() - m_audioOutput->bytesFree();
		usInBuffer = (qint64)(1000000) * bytesInBuffer / ( audio.format.channels() * audio.format.sampleSize() / 8 ) / audio.format.frequency();
		//cl_log.log("ms2 in buff = ", (int)usInBuffer/1000, cl_logALWAYS);


	

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

void CLAudioStreamDisplay::stateChanged(QAudio::State state)
{
	
	switch(state)
	{
	case QAudio::ActiveState:
		cl_log.log("audiostate=ActiveState", cl_logALWAYS);
		break;

	case QAudio::SuspendedState:
		cl_log.log("audiostate=SuspendedState", cl_logALWAYS);
		break;

	case QAudio::StoppedState:
		cl_log.log("audiostate=StoppedState", cl_logALWAYS);
	    break;
	case QAudio::IdleState:

		cl_log.log("audiostate=IdleState ", cl_logALWAYS);
	    break;
	default:
	    break;
	}

	int bytesInBuffer = m_audioOutput->bufferSize() - m_audioOutput->bytesFree();
	cl_log.log("kb2 in buff = ", bytesInBuffer/1024, cl_logALWAYS);
	/**/


}