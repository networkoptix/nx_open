#include "qtvsound.h"
#include "qtvaudiodevice.h"

#ifdef Q_OS_MAC
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <al.h>
#include <alc.h>
#endif


QtvSound::QtvSound(ALCdevice* device, uint numChannels, uint bitsPerSample, uint frequency, uint size) 
{
	m_numChannels = numChannels;
	m_frequency = frequency;
	m_bitsPerSample = bitsPerSample;
	m_size = size;
    m_proxyBuffer = new quint8[m_size];
    m_proxyBufferLen = 0;
	m_source = 0;
	m_format = 0;
	m_device = device;
	m_isValid = setup();
}

QtvSound::~QtvSound()
{
    internalClear();
    delete [] m_proxyBuffer;
}

bool QtvSound::setup() 
{
	Q_ASSERT(m_bitsPerSample && m_numChannels && m_size);

	if (!m_bitsPerSample || !m_numChannels || !m_size || !setFormat()) {
		return false;
	}
	if (!alIsSource(m_source)) {
		checkOpenALError(m_device);
		// create source for sound
		alGenSources(1, &m_source);
		checkOpenALError(m_device);
	}
	else {
		alSourceStop(m_source);
		checkOpenALError(m_device);
	}
	return true;
}



bool QtvSound::setFormat()
{
	switch(m_numChannels) {
		case 1:
			m_format = 16 == m_bitsPerSample ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;
			break;
		case 2:
			m_format = 16 == m_bitsPerSample ? AL_FORMAT_STEREO16 : AL_FORMAT_STEREO8;
			break;
		case 4:
			m_format = 16 == m_bitsPerSample ? alGetEnumValue("AL_FORMAT_QUAD16") : alGetEnumValue("AL_FORMAT_QUAD8");
			checkOpenALError(m_device);
			break;
		case 6:
			m_format = 16 == m_bitsPerSample ? alGetEnumValue("AL_FORMAT_51CHN16") : alGetEnumValue("AL_FORMAT_51CHN8");
			checkOpenALError(m_device);
			break;
		default:
			return false;
	}
	return true;
}

uint QtvSound::bitRate() const 
{
	return m_frequency * (m_bitsPerSample / 8) * m_numChannels;
}


uint QtvSound::bufferTime() const
{
    uint result = static_cast<uint>(1000000.0f * m_size / bitRate());
	return result;
}

uint QtvSound::playTimeElapsed() const
{
	ALfloat offset;
	ALint queued = 0;
	//QMutexLocker locker(&mutex);
	alGetSourcef(m_source, AL_SEC_OFFSET, &offset);
	checkOpenALErrorDebug(m_device);
	alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);
	checkOpenALErrorDebug(m_device);

    uint res = static_cast<uint>(bufferTime() * queued - offset * 1000000.0f);

	return res;
}

bool QtvSound::playImpl() const
{
	ALint state = 0;
	// get state
	alGetSourcei(m_source, AL_SOURCE_STATE, &state);
	checkOpenALErrorDebug(m_device);
	// if already playing
	if (AL_PLAYING != state) {
		// If there are Buffers in the Source Queue then the Source was starved of audio
		// data, so needs to be restarted (because there is more audio data to play)
		ALint queuedBuffers = 0;
		alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queuedBuffers);
		checkOpenALErrorDebug(m_device);
		if (queuedBuffers) {
			// play
			alSourcePlay(m_source);
			checkOpenALErrorDebug(m_device);
			return true;
		}
	}
	return false;
}

bool QtvSound::isFormatSupported()
{
    return m_isValid;
}

bool QtvSound::play(const quint8* data, uint size)
{
    while (m_proxyBufferLen == 0 && size >= m_size)
    {
        internalPlay(data, m_size);
        data += m_size;
        size -= m_size;
    }

    while (size > 0)
    {
        int copyLen = qMin(size, m_size - m_proxyBufferLen);
        memcpy(m_proxyBuffer + m_proxyBufferLen, data, copyLen);
        m_proxyBufferLen += copyLen;
        if (m_proxyBufferLen == m_size)
        {
            internalPlay(m_proxyBuffer, m_proxyBufferLen);
            m_proxyBufferLen = 0;
        }
        data += copyLen;
        size -= copyLen;
    }
    return true;
}

bool QtvSound::internalPlay(const void* data, uint size)
{
    //QMutexLocker locker(&mutex);
    ALint processed = 0;
    alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
    checkOpenALErrorDebug(m_device);
    ALuint buf = 0;
    if (processed) {
        alSourceUnqueueBuffers(m_source, 1, &buf);
        checkOpenALErrorDebug(m_device);
    }
    if (buf == 0) {
        alGenBuffers(1, &buf);
        checkOpenALErrorDebug(m_device);
        m_buffers << buf;
    }
    alBufferData(buf, m_format, data, size, m_frequency);
    checkOpenALErrorDebug(m_device);
    alSourceQueueBuffers(m_source, 1, &buf);
    checkOpenALErrorDebug(m_device);
    return playImpl();
}


bool QtvSound::outError(int err, const char* strerr) 
{
	if (strerr)
		qDebug("%s%d%s%s", "OpenAL error, code: ", err, " description: ", strerr);
	else
		qDebug("%s%d", "OpenAL error, code: ", err);
	return strerr != NULL;
}


int QtvSound::checkOpenALError(ALCdevice* device) 
{
	// получить ошибку
	int err = alGetError();
	if (err  != AL_NO_ERROR) {
		const char* strerr = alGetString(err);
		outError(err, strerr);
	}
	err = alcGetError(device);
	if (err != AL_NO_ERROR)  {
		const char* strerr = alcGetString(device, err);
		outError(err, strerr);
	}
	return err;
}

int QtvSound::checkOpenALErrorDebug(ALCdevice* device) 
{
#ifdef _DEBUG
	return checkOpenALError(device);
#else
	return 0;
#endif
}

void QtvSound::suspend()
{
    alSourcePause(m_source);
}

void QtvSound::resume()
{
	ALint state = 0;
	alGetSourcei(m_source, AL_SOURCE_STATE, &state);
	checkOpenALErrorDebug(m_device);
	if (AL_PLAYING != state) 
    {
        alSourcePlay(m_source);
    }
}

void QtvSound::clear()
{
    internalClear();
    m_isValid = setup();
}

void QtvSound::internalClear()
{
	alSourceStop(m_source);
	checkOpenALError(m_device);
	alSourceRewind(m_source);
	checkOpenALError(m_device);
	alDeleteSources(1, &m_source);
	checkOpenALError(m_device);
	if (!m_buffers.empty()) {
		alDeleteBuffers(m_buffers.size(), &m_buffers[0]);
		checkOpenALError(m_device);
	}
    m_buffers.clear();
    m_proxyBufferLen = 0;
}
