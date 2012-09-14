#include "qtvaudiodevice.h"

#include <QtCore/QScopedPointer>

#include "qtvsound.h"

#include <AL/al.h>
#include <AL/alc.h>
#include "utils/common/log.h"

//#define OPENAL_STATIC

#ifdef OPENAL_STATIC
extern "C" {
void alc_init(void);
void alc_deinit(void);
#pragma comment(lib, "winmm.lib")
}
#endif

Q_GLOBAL_STATIC(QtvAudioDevice, qtv_audioDevice_instance)

QtvAudioDevice *QtvAudioDevice::instance()
{
    return qtv_audioDevice_instance();
}

QtvAudioDevice::QtvAudioDevice(QObject *parent)
    : m_device(0), m_context(0), m_volume(1.0)
{
#ifdef OPENAL_STATIC
    alc_init();
    qDebug("OpenAL init");
#endif

    m_device = alcOpenDevice(NULL);
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
            QtvSound::checkOpenALError(m_device);

            cl_log.log("OpenAL info: ", cl_logINFO);
            cl_log.log("version: ", versionString(), cl_logINFO);
            cl_log.log("company: ", company(), cl_logINFO);
            cl_log.log("Device type: ", static_cast<const char *>(alGetString(AL_RENDERER)), cl_logINFO);
            cl_log.log("OpenAL extensions: ", static_cast<const char *>(alGetString(AL_EXTENSIONS)), cl_logINFO);
        }
    }
}

QtvAudioDevice::~QtvAudioDevice()
{
    qDeleteAll(m_sounds);

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

QString QtvAudioDevice::versionString() const {
    int majorVersion = 0;
    int minorVersion = 0;
    alcGetIntegerv(m_device, ALC_MAJOR_VERSION, 1, &majorVersion);
    alcGetIntegerv(m_device, ALC_MINOR_VERSION, 1, &minorVersion);

    return QString::number(majorVersion) + QLatin1String(".") + QString::number(minorVersion);
}

QString QtvAudioDevice::company() const {
    return QLatin1String(static_cast<const char *>(alGetString(AL_VENDOR)));
}

float QtvAudioDevice::volume() const
{
    return qBound(0.0f, m_volume, 1.0f);
}

void QtvAudioDevice::setVolume(float volume)
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
    foreach (QtvSound *sound, m_sounds)
        sound->setVolumeLevel(volume);

    emit volumeChanged();
}

bool QtvAudioDevice::isMute() const
{
    return volume() == 0.0f;
}

void QtvAudioDevice::setMute(bool mute)
{
    setVolume(!mute ? (m_volume < 0.0f ? -m_volume : m_volume) : 0.0f);
}

void QtvAudioDevice::removeSound(QtvSound *soundObject)
{
    if (!soundObject)
        return;

    m_sounds.removeOne(soundObject);
    delete soundObject;
}

QtvSound *QtvAudioDevice::addSound(const QnAudioFormat &format)
{
    if (!m_device || !m_context)
        return 0;

    QScopedPointer<QtvSound> sound(new QtvSound(m_device, format));
    if (sound->isValid())
    {
        sound->setVolumeLevel(m_volume);
        m_sounds.append(sound.data());
        return sound.take();
    }

    return 0;
}
