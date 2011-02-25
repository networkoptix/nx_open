#include "audio_device.h"
#include "base\log.h"
#include "base\ringbuffer.h"

// to avoid situation than all data goes from buffered audio to qt buffer at one shot 
// qt buffer size must be much smaller than buffered audio size
static const int QT_AUDIO_BUFFER_SIZE = 200;

CLAudioDevice::CLAudioDevice(QAudioFormat format):
m_downmixing(false),
m_audioOutput(0),
m_audiobuff(0),
m_ringbuff(0)
{
    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());


    bool supported = true;

    if (!info.isFormatSupported(format)) 
    {

        if (format.channels()>2)
        {
            format.setChannels(2);
            if (!info.isFormatSupported(format)) 
            {
                cl_log.log("audio format not supported by backend, cannot play audio.", cl_logERROR);
                supported = false;
            }

            m_downmixing = true;

        }
    }

    if (supported)
    {
        m_audioOutput = new QAudioOutput(format);
        m_audioOutput->setBufferSize( bytes_from_time(format, QT_AUDIO_BUFFER_SIZE) );

        m_ringbuff = new CLRingBuffer( bytes_from_time(format, 1000) ); // I assume one packet will never contain more than one 700 ms of decompressed sound

        m_audiobuff = m_audioOutput->start();
    }

    m_format = format;

}

CLAudioDevice::~CLAudioDevice()
{
    if (m_audioOutput)
    {
        m_audioOutput->stop();
        delete m_audioOutput;
        m_audioOutput = 0;

        delete m_ringbuff;
    }
}


bool CLAudioDevice::downmixing() const
{
    return m_downmixing;
}


bool CLAudioDevice::wantMoreData() 
{
    int moreChunks = tryMoveDatafromRingBufferToQtBuffer(); 
    //if moreChunks>0  => ring buffer do not have enought data to move it to the qt buffer



    //return (moreChunks>0);

    return (m_ringbuff->bytesAvailable() < m_ringbuff->capacity()/4); // 7 is luky number
}


void CLAudioDevice::write(const char* data, unsigned long size)
{
    // firs of all try to write all data to ring buffer 
    size = qMin (m_ringbuff->avalable_to_write(), (qint64)size);
    m_ringbuff->writeData(data,size);

    // and
    tryMoveDatafromRingBufferToQtBuffer();
}


int CLAudioDevice::msInBuffer() const
{
    if (!m_audioOutput)
        return 0;

    int bytes = (m_audioOutput->bufferSize() - m_audioOutput->bytesFree()) + m_ringbuff->bytesAvailable();

    //int t  = ms_from_size(m_format, bytes);

    return ms_from_size(m_format, bytes);
}


void CLAudioDevice::suspend()
{
    if (!m_audioOutput)
        return;

    m_audioOutput->suspend();
}

void CLAudioDevice::resume()
{
    if (!m_audioOutput)
        return;

    m_audioOutput->resume();
}

void CLAudioDevice::clearAudioBuff()
{
	if (m_ringbuff) 
		m_ringbuff->clear();


    // this takes a lot of time of ui thread. let's do not care about cleaning qt audio buff
    /*
	if (m_audioOutput)
	{
		m_audioOutput->reset();
		m_audioOutput->start();
	}
    /**/
}


//=============================================================


unsigned int CLAudioDevice::ms_from_size(const QAudioFormat& format, unsigned long bytes) const
{
    return (qint64)(1000) * bytes/ ( format.channels() * format.sampleSize() / 8 ) / format.frequency();
}

unsigned int CLAudioDevice::bytes_from_time(const QAudioFormat& format, unsigned long ms) const
{
    return ( (qint64)format.channels() * format.sampleSize() / 8 ) * format.frequency() * ms / 1000;
}


int CLAudioDevice::tryMoveDatafromRingBufferToQtBuffer()
{

    if (!m_audioOutput)
        return 0;


    int period = m_audioOutput->periodSize();
    int ring_avalable = m_ringbuff->bytesAvailable();
    int output_avalable = m_audioOutput->bytesFree();

    int chunks = qMin(ring_avalable, output_avalable) / period;


    while (chunks)
    {
        m_ringbuff->readToIODevice(m_audiobuff, period);
        --chunks;
    }

    return m_audioOutput->bytesFree()/period;
}