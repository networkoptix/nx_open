#include "qtvaudiodevice.h"

#include <QtCore/QSettings>

#include <QtMultimedia/QAudioFormat>

#include "qtvsound.h"

#include <OpenAL/al.h>
#include <OpenAL/alc.h>

//#define OPENAL_STATIC

//#include <algorithm>
#ifdef OPENAL_STATIC
extern "C" {
void alc_init(void);
void alc_deinit(void);
#pragma comment(lib, "winmm.lib")
//"msvcrt.lib"
}
#endif

Q_GLOBAL_STATIC(QtvAudioDevice, getAudioDevice)

QtvAudioDevice &QtvAudioDevice::instance()
{
    return *getAudioDevice();
}

QtvAudioDevice::QtvAudioDevice()
{
    m_settings = new QSettings;
    m_settings->beginGroup(QLatin1String("audioControl"));

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

            // get version
            int majorVersion = 0;
            int minorVersion = 0;
            alcGetIntegerv(m_device, ALC_MAJOR_VERSION, 1, &majorVersion);
            alcGetIntegerv(m_device, ALC_MINOR_VERSION, 1, &minorVersion);
            // Check what device and version we are using
            const char *name = alcGetString(m_device, ALC_DEVICE_SPECIFIER);
            qDebug("%s%s%s%d%s%d", "Opened, ", name ? name : "", " spec version ", majorVersion, ".", minorVersion);
            qDebug("%s%s", "Company: ", static_cast<const char *>(alGetString(AL_VENDOR)));
            qDebug("%s%s", "Device type: ", static_cast<const char *>(alGetString(AL_RENDERER)));
            qDebug("%s%s", "OpenAL extensions: ", static_cast<const char *>(alGetString(AL_EXTENSIONS)));
            qDebug("%s", "OpenAL init ok");
        }
    }
}

QtvAudioDevice::~QtvAudioDevice()
{
    m_settings->endGroup();
    delete m_settings;

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

float QtvAudioDevice::volume() const
{
    float volume = m_settings->value(QLatin1String("volume"), 1.0f).toFloat();
    return qBound(0.0f, volume, 1.0f);
}

void QtvAudioDevice::setVolume(float volume)
{
    volume = qBound(0.0f, volume, 1.0f);
    float oldVolume = m_settings->value(QLatin1String("volume"), 1.0f).toFloat();
    if (volume == oldVolume)
        return;

    if (volume == 0.0f)
    {
        if (oldVolume < volume)
            return;

        volume = -oldVolume;
    }

    m_settings->setValue(QLatin1String("volume"), volume);
    m_settings->sync();

    volume = qBound(0.0f, volume, 1.0f);
    foreach (QtvSound *sound, m_sounds)
        sound->setVolumeLevel(volume);
}

bool QtvAudioDevice::isMute() const
{
    return volume() == 0.0f;
}

void QtvAudioDevice::setMute(bool mute)
{
    float volume;
    if (mute)
    {
        volume = 0.0f;
    }
    else
    {
        volume = m_settings->value(QLatin1String("volume"), 1.0f).toFloat();
        if (volume < 0.0f)
            volume = -volume;
        else if (volume == 0.0f)
            volume = 1.0f;
    }
    setVolume(volume);
}

void QtvAudioDevice::removeSound(QtvSound *soundObject)
{
    if (!soundObject)
        return;

    bool isRemoved = m_sounds.removeOne(soundObject);
    Q_ASSERT(isRemoved); Q_UNUSED(isRemoved)
    delete soundObject;
}

QtvSound *QtvAudioDevice::addSound(const QAudioFormat &format)
{
    if (!m_device || !m_context)
        return 0;

    QtvSound *result = new QtvSound(m_device, format);
    if (!result->isValid())
    {
        delete result;
        return 0;
    }

    m_sounds.push_back(result);
    return result;
}
