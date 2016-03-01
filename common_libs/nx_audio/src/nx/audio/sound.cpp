#include "sound.h"

#ifdef Q_OS_MACX
#include <openal/al.h>
#include <openal/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include "utils/common/sleep.h"
#include "audiodevice.h"
#include <nx/utils/log/assert.h>

namespace nx {
namespace audio {


Sound::Sound(ALCdevice *device, const QnAudioFormat& audioFormat):
    QObject()
{
    m_audioFormat = audioFormat;
    m_numChannels = audioFormat.channelCount();
    m_frequency = audioFormat.sampleRate();
    m_bitsPerSample = audioFormat.sampleSize();
    m_size = bitRate() / 30; // use 33 ms buffers

    // Multiply by 2 to align OpenAL buffer
    int sampleSize = 2 * audioFormat.channelCount() * audioFormat.sampleSize() / 8;
    if (m_size % sampleSize)
        m_size += sampleSize - (m_size % sampleSize);

    m_proxyBuffer = new quint8[m_size];
    m_proxyBufferLen = 0;
    m_source = 0;
    m_format = 0;
    m_device = device;
    m_deinitialized = false;
    m_isValid = setup();
    m_paused = false;

    connect(AudioDevice::instance(), &AudioDevice::volumeChanged, this, [this] (float value)
    {
        setVolumeLevel(value);
    });

}

Sound::~Sound()
{
    if (!m_deinitialized)
        internalClear();
    delete [] m_proxyBuffer;
}

bool Sound::setup()
{
    NX_ASSERT(m_bitsPerSample && m_numChannels && m_size);
    if (!m_bitsPerSample || !m_numChannels || !m_size )
        return false;

    m_format = getOpenAlFormat(m_audioFormat);
    if (m_format == 0)
        return false;

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

float Sound::volumeLevel() const
{
    ALfloat volume = 0.0;
    QnMutexLocker lock( &m_mtx );
    alGetSourcef(m_source, AL_GAIN, &volume);
    return volume;
}

void Sound::setVolumeLevel(float volumeLevel)
{
    QnMutexLocker lock( &m_mtx );
    alSourcef(m_source, AL_GAIN, volumeLevel);
}

int Sound::getOpenAlFormat(const QnAudioFormat &audioFormat)
{
    if (audioFormat.sampleType() == QnAudioFormat::Float)
        return false;

    QByteArray requestFormat;
    int bitsPerSample = audioFormat.sampleSize();
    int numChannels = audioFormat.channelCount();
    int format = 0;
    switch (numChannels)
    {
        case 1:
            if (32 == bitsPerSample)
            {
                // format detected, but audio does not play. So, mark format as unsupported
                format = 0; // alGetEnumValue("AL_FORMAT_MONO32");
            }
            else if (16 == bitsPerSample)
                format = AL_FORMAT_MONO16;
            else if (8 == bitsPerSample)
                format = AL_FORMAT_MONO8;
            break;
        case 2:
            if (32 == bitsPerSample)
            {
                // format detected, but audio does not play. So, mark format as unsupported
                format = 0; //alGetEnumValue("AL_FORMAT_STEREO32");
            }
            else if (16 == bitsPerSample)
                format = AL_FORMAT_STEREO16;
            else if (8 == bitsPerSample)
                format = AL_FORMAT_STEREO8;
            break;
        case 4:
            requestFormat = "AL_FORMAT_QUAD" + QByteArray::number(bitsPerSample);
            format = alGetEnumValue(requestFormat);
            break;
        default:
            requestFormat = "AL_FORMAT_" + QByteArray::number(numChannels - 1) +
                            "1CHN" + QByteArray::number(bitsPerSample);
            format = alGetEnumValue(requestFormat);
            break;
    }
    return format;
}

uint Sound::bitRate() const
{
    return m_frequency * (m_bitsPerSample / 8) * m_numChannels;
}

uint Sound::bufferTime() const
{
    uint result = static_cast<uint>(1000000.0f * m_size / bitRate());
    return result;
}

void Sound::clearBuffers(bool clearAll)
{
    ALint processed = 0;
    if (clearAll)
        alGetSourcei(m_source, AL_BUFFERS_QUEUED, &processed);
    else
        alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
    checkOpenALErrorDebug(m_device);
    if (processed)
    {
        processed = qMin(processed, static_cast<ALint>(sizeof(m_tmpBuffer) / sizeof(m_tmpBuffer[0])));
        alSourceUnqueueBuffers(m_source, processed, m_tmpBuffer);
        checkOpenALErrorDebug(m_device);
        if (alGetError() == AL_NO_ERROR)
        {
            alDeleteBuffers(processed, m_tmpBuffer);
            checkOpenALErrorDebug(m_device);
        }
    }
}

uint Sound::playTimeElapsedUsec()
{
    QnMutexLocker lock( &m_mtx );

    if (m_deinitialized)
        return 0;

    clearBuffers(false);

    ALfloat offset;
    ALint queued = 0;
    alGetSourcef(m_source, AL_SEC_OFFSET, &offset);
    checkOpenALErrorDebug(m_device);
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);
    checkOpenALErrorDebug(m_device);

    uint res = static_cast<uint>(bufferTime() * queued - offset * 1000000.0f);

    uint unbufferedDuration = (m_proxyBufferLen * 1000000) / bitRate();
    return res +unbufferedDuration;
}

bool Sound::isBufferUnderflow()
{
    QnMutexLocker lock(&m_mtx);

    if (m_deinitialized)
        return false;

    clearBuffers(false);

    ALint queued = 0;
    checkOpenALErrorDebug(m_device);
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);
    checkOpenALErrorDebug(m_device);

    return queued == 0;
}

bool Sound::playImpl()
{
    ALint state = 0;
    // get state
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    checkOpenALErrorDebug(m_device);
    // if already playing
    if (AL_PLAYING != state) {
        float volume = AudioDevice::instance()->volume();
        alSourcef(m_source, AL_GAIN, volume);

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
    return true;
}

bool Sound::isFormatSupported(const QnAudioFormat& format)
{
    return getOpenAlFormat(format) != 0;
}

QAudio::State Sound::state() const
{
    if (m_paused)
        return QAudio::SuspendedState;
    else if (m_isValid)
        return QAudio::ActiveState;
    else
        return QAudio::StoppedState;
}

bool Sound::write(const quint8* data, uint size)
{
    QnMutexLocker lock( &m_mtx );

    if (m_deinitialized)
    {
        m_isValid = setup();
        m_deinitialized = false;
    }

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
        if (m_proxyBufferLen == (int)m_size)
        {
            internalPlay(m_proxyBuffer, m_size);
            m_proxyBufferLen = 0;
        }
        data += copyLen;
        size -= copyLen;
    }
    return true;
}

bool Sound::internalPlay(const void* data, uint size)
{
    clearBuffers(false);
    ALuint buf = 0;
    alGenBuffers(1, &buf);
    checkOpenALErrorDebug(m_device);
    alBufferData(buf, m_format, data, size, m_frequency);
    checkOpenALErrorDebug(m_device);
    alSourceQueueBuffers(m_source, 1, &buf);
    checkOpenALErrorDebug(m_device);
    return m_paused ? true : playImpl();
}

bool Sound::outError(int err, const char* strerr)
{
    if (strerr)
        qDebug("%s%d%s%s", "OpenAL error, code: ", err, " description: ", strerr);
    else
        qDebug("%s%d", "OpenAL error, code: ", err);
    return strerr != NULL;
}

int Sound::checkOpenALError(ALCdevice *device)
{
    // get an error
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

int Sound::checkOpenALErrorDebug(ALCdevice *device)
{
#ifdef _DEBUG
    return checkOpenALError(device);
#else
    Q_UNUSED(device)
    return 0;
#endif
}

void Sound::suspend()
{
    QnMutexLocker lock( &m_mtx );
    m_paused = true;
    alSourcePause(m_source);
}

void Sound::resume()
{
    QnMutexLocker lock( &m_mtx );
    m_paused = false;
    playImpl();
}

void Sound::clear()
{
    QnMutexLocker lock( &m_mtx );
    if (m_deinitialized)
        return;
    internalClear();
    m_deinitialized = true;
    m_paused = false;
}

void Sound::internalClear()
{
    alSourceStop(m_source);
    checkOpenALError(m_device);

#ifdef Q_OS_MAC
    QnSleep::msleep(bufferTime()/1000);
#endif

    clearBuffers(true);
    alDeleteSources(1, &m_source);
    checkOpenALError(m_device);
    m_proxyBufferLen = 0;
    m_source = 0;
}

QnAudioFormat Sound::format() const
{
    return m_audioFormat;
}

} // namespace audio
} // namespace nx
