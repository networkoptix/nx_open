#include "audio_device.h"
#include "base\log.h"
#include "base\ringbuffer.h"

/**
  * QT audio buffer size.
  * Note: to avoid situation than all data goes from buffered audio to qt buffer at one shot 
  * qt buffer size must be much smaller than buffered audio size
  */
static const int QT_AUDIO_BUFFER_SIZE = 300;

CLAudioDevice::CLAudioDevice(QAudioFormat format)
  : m_downmixing(false),
    m_audioOutput(0),
    m_audioBuffer(0),
    m_ringBuffer(0)
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
        m_audioOutput->setBufferSize( bytesFromTime(format, QT_AUDIO_BUFFER_SIZE) );

        m_ringBuffer = new CLRingBuffer( bytesFromTime(format, 1500) ); // I assume one packet will never contain more than one 700 ms of decompressed sound

        m_audioBuffer = m_audioOutput->start();
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

        delete m_ringBuffer;
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

    return (m_ringBuffer->bytesAvailable() < m_ringBuffer->capacity()/2); // 7 is luky number
}

void CLAudioDevice::write(const char* data, unsigned long size)
{
    // firs of all try to write all data to ring buffer 
    size = qMin (m_ringBuffer->avalable_to_write(), (qint64)size);
    m_ringBuffer->writeData(data,size);

    // and
    tryMoveDatafromRingBufferToQtBuffer();
}

int CLAudioDevice::msInBuffer() const
{
    if (!m_audioOutput)
        return 0;

    int bytes = (m_audioOutput->bufferSize() - m_audioOutput->bytesFree()) + m_ringBuffer->bytesAvailable();

    //int t  = ms_from_size(m_format, bytes);

    return msFromSize(m_format, bytes);
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
	if (m_ringBuffer) 
		m_ringBuffer->clear();

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

unsigned int CLAudioDevice::msFromSize(const QAudioFormat& format, unsigned long bytes) const
{
    return (qint64)(1000) * bytes/ ( format.channels() * format.sampleSize() / 8 ) / format.frequency();
}

unsigned int CLAudioDevice::bytesFromTime(const QAudioFormat& format, unsigned long ms) const
{
    return ( (qint64)format.channels() * format.sampleSize() / 8 ) * format.frequency() * ms / 1000;
}

int CLAudioDevice::tryMoveDatafromRingBufferToQtBuffer()
{
    if (!m_audioOutput)
        return 0;

    int period = m_audioOutput->periodSize();
    int ringAvailable = m_ringBuffer->bytesAvailable();
    int outputAvailable = m_audioOutput->bytesFree();

    int chunks = qMin(ringAvailable, outputAvailable) / period;

    while (chunks)
    {
        m_ringBuffer->readToIODevice(m_audioBuffer, period);
        --chunks;
    }

    return m_audioOutput->bytesFree() / period;
}