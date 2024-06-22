// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

namespace nx {
namespace audio {

class Sound;
struct Format;

class AudioDevice: public QObject
{
    Q_OBJECT

public:
    static AudioDevice* instance();

    AudioDevice(QObject* parent = nullptr);
    ~AudioDevice();

    Sound* createSound(const Format& format) const;

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

    static int internalBufferInSamples(void* device);
    void initDeviceInternal();

signals:
    void volumeChanged(float value);

private:
    void* m_device = nullptr;
    void* m_context = nullptr;
    float m_volume = 1.0;
};

} // namespace audio
} // namespace nx
