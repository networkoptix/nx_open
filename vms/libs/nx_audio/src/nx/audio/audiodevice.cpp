// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audiodevice.h"

#include <QtCore/QScopedPointer>

#include "audio.h"
#include "sound.h"

#if defined(Q_OS_MAC)
    #include <openal/al.h>
    #include <openal/alc.h>
#else
    #include <AL/al.h>
    #include <AL/alc.h>
    #if defined(Q_OS_WINDOWS)
        #include <AL/alext.h>
    #endif
#endif
#include <nx/utils/log/log.h>

#if defined(OPENAL_STATIC)
    extern "C"
    {
    void alc_init(void);
    void alc_deinit(void);
    #pragma comment(lib, "winmm.lib")
    }
#endif

#include <nx/media/audio/format.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/ios_device_info.h>
#include <utils/common/delayed.h>

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

#if defined(Q_OS_WINDOWS)
    static LPALCREOPENDEVICESOFT alcReopenDeviceSOFT = nullptr;
#endif

} // unnamed namespace

#if defined(Q_OS_ANDROID)
    int AudioDevice::internalBufferInSamples(ALCdevice* device)
    {
        const ALCdevice_struct* devicePriv = (const ALCdevice_struct*) device;
        return devicePriv->UpdateSize;
    }
#endif

AudioDevice *AudioDevice::instance()
{
    static AudioDevice audioDevice;
    return &audioDevice;
}

#if defined(Q_OS_ANDROID)
    void AudioDevice::initDeviceInternal()
    {
        // Android opensl backend has quite big audio jitter and current position epsilon.
        // Update buffer to smaller size to reduce they. Default value is 1024
        ALCdevice_struct* devicePriv = (ALCdevice_struct*) m_device;
        static const qint64 kOpenAlBufferSize = 512;
        devicePriv->UpdateSize = kOpenAlBufferSize;
    }
#endif

AudioDevice::AudioDevice(QObject* parent):
    QObject(parent)
{
    #if defined(OPENAL_STATIC)
        alc_init();
        NX_DEBUG(this, "OpenAL init");
    #endif

    nx::audio::setupAudio();

    m_device = alcOpenDevice(nullptr);
    #if defined(Q_OS_ANDROID)
        initDeviceInternal();
    #endif

    if (!m_device)
    {
        NX_ERROR(this, "Unable to open device");
        return;
    }

    #if defined(Q_OS_WINDOWS)
        setupReopenCallback();
    #endif

    m_context = alcCreateContext(m_device, nullptr);
    if (!m_context)
    {
        NX_ERROR(this, "Unable to create context");
        return;
    }

    alcMakeContextCurrent(m_context);
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);

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

Sound* AudioDevice::createSound(const nx::media::audio::Format& format) const
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

#if defined(Q_OS_WINDOWS)
    void AudioDevice::setupReopenCallback()
    {
        LPALCEVENTCONTROLSOFT alcEventControlSOFT;
        LPALCEVENTCALLBACKSOFT alcEventCallbackSOFT;

        alcReopenDeviceSOFT = reinterpret_cast<LPALCREOPENDEVICESOFT>(
            alcGetProcAddress(m_device, "alcReopenDeviceSOFT"));
        alcEventControlSOFT = reinterpret_cast<LPALCEVENTCONTROLSOFT>(
            alGetProcAddress("alcEventControlSOFT"));
        alcEventCallbackSOFT = reinterpret_cast<LPALCEVENTCALLBACKSOFT>(
            alGetProcAddress("alcEventCallbackSOFT"));

        if (alcReopenDeviceSOFT && alcEventControlSOFT && alcEventCallbackSOFT)
        {
            const std::array<ALenum,1> evt_types{{ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT}};
            alcEventControlSOFT(static_cast<ALsizei>(evt_types.size()), evt_types.data(), AL_TRUE);

            alcEventCallbackSOFT(
                [](
                    ALCenum eventType,
                    ALCenum,
                    ALCdevice*,
                    ALCsizei,
                    const ALCchar*,
                    void* userParam
                ) noexcept -> void
                {
                    if (eventType != ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT)
                        return;

                    auto audioDevice = reinterpret_cast<AudioDevice*>(userParam);

                    auto reopenDevice = nx::utils::guarded(audioDevice,
                        [audioDevice]()
                        {
                            alcReopenDeviceSOFT(audioDevice->m_device, nullptr, nullptr);
                        });

                    executeLaterInThread(reopenDevice, audioDevice->thread());
                },
                reinterpret_cast<void*>(this));
        }
        else
        {
            NX_WARNING(this, "Reopen or default device changing detection is not supported.");
        }
    }
#endif

} // namespace audio
} // namespace nx
