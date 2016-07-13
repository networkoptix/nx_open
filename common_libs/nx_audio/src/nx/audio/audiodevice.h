#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <utils/media/audioformat.h>

typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;

namespace nx {
namespace audio {

class Sound;

class AudioDevice: public QObject
{
    Q_OBJECT

public:
    static AudioDevice* instance();

    AudioDevice(QObject* parent = nullptr);
    ~AudioDevice();

    Sound* createSound(const QnAudioFormat& format);

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
    QString company() const;

private:
    friend class Sound;

    static int internalBufferInSamples(ALCdevice* device);
    void initDeviceInternal();

signals:
    void volumeChanged(float value);

private:
    ALCdevice* m_device;
    ALCcontext* m_context;
    float m_volume;
};

} // namespace audio
} // namespace nx
