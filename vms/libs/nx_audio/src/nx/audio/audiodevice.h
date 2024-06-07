// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

#if defined(Q_OS_MAC) || defined(Q_OS_IOS)
    using ALCdevice = struct ALCdevice_struct;
    using ALCcontext = struct ALCcontext_struct;
#else
    struct ALCdevice;
    struct ALCcontext;
#endif

namespace nx::media::audio { struct Format; }

namespace nx::audio {

class Sound;

class AudioDevice: public QObject
{
    Q_OBJECT

public:
    static AudioDevice* instance();

    AudioDevice(QObject* parent = nullptr);
    ~AudioDevice();

    void deinitialize();

    Sound* createSound(const nx::media::audio::Format& format) const;

    /**
     * @return volume level in range [0..1].
     */
    float volume() const;

    /**
     * Set volume level. This call affects volume for all created audio devices.
     */
    void setVolume(float value);

    /**
     * @return True if audio is mutted.
     */
    bool isMute() const;

    /**
     * Mute/unmute the audio.
     */
    void setMute(bool mute);

    QString versionString() const;
    static QString company();

private:
    friend class Sound;

    #if defined(Q_OS_ANDROID)
        static int internalBufferInSamples(ALCdevice* device);
        void initDeviceInternal();
    #endif

private:
    #if defined(Q_OS_WINDOWS)
        void setupReopenCallback();
    #endif

signals:
    void volumeChanged(float value);

private:
    ALCdevice* m_device = nullptr;
    ALCcontext* m_context = nullptr;
    float m_volume = 1.0;
};

} // namespace nx::audio
