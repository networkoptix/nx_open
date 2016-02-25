#include "audiodevice.h"

#include <QtCore/QScopedPointer>

#include "sound.h"

#ifdef Q_OS_MACX
#include <openal/al.h>
#include <openal/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include "utils/common/log.h"

#ifdef OPENAL_STATIC
extern "C" {
void alc_init(void);
void alc_deinit(void);
#pragma comment(lib, "winmm.lib")
}
#endif

namespace nx {
namespace audio {

AudioDevice *AudioDevice::instance()
{
    static AudioDevice audioDevice;
    return &audioDevice;
}

AudioDevice::AudioDevice(QObject *parent)
    : m_device(0), m_context(0), m_volume(1.0)
{
    Q_UNUSED(parent)
#ifdef OPENAL_STATIC
    alc_init();
    qDebug("OpenAL init");
#endif

    m_device = alcOpenDevice(NULL);
    quint32* gggData = (quint32*)m_device;
    gggData[5] = 1; //< override num_updates field

    if (!m_device)
    {
        qWarning("OpenAL: unable to open Device");
    }
    else
    {
        m_context = alcCreateContext(m_device, NULL);
        if (!m_context)
        {
            qWarning("OpenAL: unable to create Context");
        }
        else
        {
            alcMakeContextCurrent(m_context);
            alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);

            cl_log.log("OpenAL info: ", cl_logINFO);
            cl_log.log("version: ", versionString(), cl_logINFO);
            cl_log.log("company: ", company(), cl_logINFO);
            cl_log.log("Device type: ", static_cast<const char *>(alGetString(AL_RENDERER)), cl_logINFO);
            cl_log.log("OpenAL extensions: ", static_cast<const char *>(alGetString(AL_EXTENSIONS)), cl_logINFO);
        }
    }
}

AudioDevice::~AudioDevice()
{
    if (m_device) {
        // Disable context
        if (m_context) {
            alcMakeContextCurrent(NULL);
            // Release context(s)
            alcDestroyContext(m_context);
            m_context = NULL;
        }

        // Close device
        alcCloseDevice(m_device);
        m_device = NULL;
    }

#ifdef OPENAL_STATIC
    alc_deinit();
    qDebug("OpenAL deinit");
#endif
}

QString AudioDevice::versionString() const {
    int majorVersion = 0;
    int minorVersion = 0;
    alcGetIntegerv(m_device, ALC_MAJOR_VERSION, 1, &majorVersion);
    alcGetIntegerv(m_device, ALC_MINOR_VERSION, 1, &minorVersion);

    return QString::number(majorVersion) + QLatin1String(".") + QString::number(minorVersion);
}

QString AudioDevice::company() const {
    return QLatin1String(static_cast<const char *>(alGetString(AL_VENDOR)));
}

float AudioDevice::volume() const
{
    return qBound(0.0f, m_volume, 1.0f);
}

void AudioDevice::setVolume(float volume)
{
    volume = qBound(0.0f, volume, 1.0f);
    if (m_volume == volume)
        return;

    if (volume == 0.0f)
    {
        if (m_volume < volume)
            return;

        volume = -m_volume;
    }

    m_volume = volume;

    volume = qBound(0.0f, volume, 1.0f);
    emit volumeChanged(volume);
}

bool AudioDevice::isMute() const
{
    return volume() == 0.0f;
}

void AudioDevice::setMute(bool mute)
{
    setVolume(!mute ? (m_volume < 0.0f ? -m_volume : m_volume) : 0.0f);
}

Sound *AudioDevice::createSound(const QnAudioFormat &format)
{
    if (!m_device || !m_context)
        return 0;

    QScopedPointer<Sound> sound(new Sound(m_device, format));
    if (sound->isValid())
    {
        sound->setVolumeLevel(m_volume);
        return sound.take();
    }

    return 0;
}

} // namespace audio
} // namespace nx
