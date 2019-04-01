#include "audiodevice.h"

#include <QtCore/QScopedPointer>

#include "sound.h"

#ifdef Q_OS_MAC
#include <openal/al.h>
#include <openal/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include <nx/utils/log/log.h>

#ifdef OPENAL_STATIC
extern "C" {
void alc_init(void);
void alc_deinit(void);
#pragma comment(lib, "winmm.lib")
}
#endif

#include <utils/common/app_info.h>
#include <nx/utils/software_version.h>
#include <nx/utils/ios_device_info.h>

namespace nx {
namespace audio {

namespace {

// openal data from private header alMain.h
// It's given from openAL version 1.17
struct ALCdevice_struct
{
    uint refCnt;
    ALCboolean Connected;
    enum DeviceType {Playback,Capture, Loopback } Type;
    ALuint       Frequency;
    ALuint       UpdateSize;
    ALuint       NumUpdates;
};

// After 11.4 iOS version we have problems with low volume level.
// Looks like it is because of a lot of fixes related to the audio
// are made in 11.4. As we haven't been able to find out what is
// wrong/changed, it is decided to make this workaround.
void fixVolumeLevel()
{
    if (!QnAppInfo::isIos())
        return;

    if (nx::utils::SoftwareVersion(QSysInfo::productVersion()) < nx::utils::SoftwareVersion(11, 4))
        return;


    const auto& deviceInfo = nx::utils::IosDeviceInformation::currentInformation();
    const bool isIPhone6 = deviceInfo.majorVersion == nx::utils::IosDeviceInformation::iPhone6;
    static const int kTimesGain = isIPhone6
        ? 4 //< Prevents volume overload in iPhone 6.
        : 64;
    alListenerf(AL_GAIN, kTimesGain);
}

} // unnamed namespace

int AudioDevice::internalBufferInSamples(ALCdevice* device)
{
    #if defined(Q_OS_MACX) || defined(Q_OS_IOS)
        /**
         * Looks like Mac OS has a bug in the standard OpenAL implementation.
         * It returns invalid pointer with alcOpenDevice (0x18) and alcCreateContext(0x19).
         * As it is invalid pointer, we can't use it accessing ALCdevice_struct' fields.
         * From the other side we can continue to use it as opaque handle for OpenAL functions.
         *
         * Problem is known:
         * https://github.com/hajimehoshi/ebiten/issues/195
         * https://groups.google.com/forum/#!topic/golang-bugs/nARJpJCYum4
         *
         * Possible workarounds are:
         * 1. Fix OpenAL sources and rebuild it for Mac OS;
         * 2. Do not use direct access to ALCdevice_struct' fields.
         *
         * Since we have correct handling of zero return value, it was decided to
         * workaround it with second option.
         */
        return 0;
    #else
        const ALCdevice_struct* devicePriv = (const ALCdevice_struct*) device;
        return devicePriv->UpdateSize;
    #endif
}

AudioDevice *AudioDevice::instance()
{
    static AudioDevice audioDevice;
    return &audioDevice;
}

void AudioDevice::initDeviceInternal()
{
#ifdef Q_OS_ANDROID
    // Android opensl backend has quite big audio jitter and current position epsilon.
    // Update buffer to smaller size to reduce they. Default value is 1024
    ALCdevice_struct* devicePriv = (ALCdevice_struct*) m_device;
    static const qint64 kOpenAlBufferSize = 512;
    devicePriv->UpdateSize = kOpenAlBufferSize;
#endif
}

AudioDevice::AudioDevice(QObject* parent):
    QObject(parent)
{
    #ifdef OPENAL_STATIC
        alc_init();
        NX_DEBUG(this, "OpenAL init");
    #endif

    m_device = alcOpenDevice(nullptr);
    initDeviceInternal();

    if (!m_device)
    {
        NX_ERROR(this, "Unable to open device");
        return;
    }

    m_context = alcCreateContext(m_device, nullptr);
    if (!m_context)
    {
        NX_ERROR(this, "Unable to create context");
        return;
    }

    alcMakeContextCurrent(m_context);
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);

    fixVolumeLevel();

    const QByteArray renderer = static_cast<const char*>(alGetString(AL_RENDERER));
    const QByteArray extensions = static_cast<const char*>(alGetString(AL_EXTENSIONS));

    NX_INFO(this, "Version: %1", versionString());
    NX_INFO(this, "Company: %1", company());
    NX_INFO(this, "Device type: %1", renderer);
    NX_DEBUG(this, "Extensions: %1", extensions);
}

AudioDevice::~AudioDevice()
{
    if (m_device)
    {
        // Disable context
        if (m_context)
        {
            alcMakeContextCurrent(nullptr);
            // Release context(s)
            alcDestroyContext(m_context);
            m_context = nullptr;
        }

        // Close device
        alcCloseDevice(m_device);
        m_device = nullptr;
    }

    #ifdef OPENAL_STATIC
        alc_deinit();
        NX_DEBUG(this, "OpenAL deinit");
    #endif
}

QString AudioDevice::versionString() const
{
    if (!m_device)
        return QString();

    int majorVersion = 0;
    int minorVersion = 0;
    alcGetIntegerv(m_device, ALC_MAJOR_VERSION, 1, &majorVersion);
    alcGetIntegerv(m_device, ALC_MINOR_VERSION, 1, &minorVersion);

    return QString::number(majorVersion) + QLatin1String(".") + QString::number(minorVersion);
}

QString AudioDevice::company()
{
    return QLatin1String(static_cast<const char *>(alGetString(AL_VENDOR)));
}

float AudioDevice::volume() const
{
    return qBound(0.0f, m_volume, 1.0f);
}

void AudioDevice::setVolume(float value)
{
    value = qBound(0.0f, value, 1.0f);
    if (m_volume == value)
        return;

    if (value == 0.0f)
    {
        if (m_volume < value)
            return;

        value = -m_volume;
    }

    m_volume = value;

    value = qBound(0.0f, value, 1.0f);
    emit volumeChanged(value);
}

bool AudioDevice::isMute() const
{
    return volume() == 0.0f;
}

void AudioDevice::setMute(bool mute)
{
    setVolume(!mute ? (m_volume < 0.0f ? -m_volume : m_volume) : 0.0f);
}

Sound* AudioDevice::createSound(const QnAudioFormat& format) const
{
    if (!m_device || !m_context)
        return nullptr;

    QScopedPointer<Sound> sound(new Sound(m_device, format));
    if (sound->isValid())
    {
        sound->setVolumeLevel(m_volume);
        return sound.take();
    }

    return nullptr;
}

} // namespace audio
} // namespace nx
