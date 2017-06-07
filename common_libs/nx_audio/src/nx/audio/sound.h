#pragma once

#include <QtCore/QVector>
#include <QtMultimedia/QAudio>
#include <QtMultimedia/QAudioFormat>

#include <nx/utils/thread/mutex.h>
#include <utils/media/audioformat.h>

#include <utils/timer.h>
#include <utils/common/safe_direct_connection.h>

class AudioDevice;
typedef struct ALCdevice_struct ALCdevice;

namespace nx {
namespace audio {

/**
 * Plays audio via openAL.
 */
class Sound: public QObject, public Qn::EnableSafeDirectConnection
{
public:
    Sound(ALCdevice* device, const QnAudioFormat& audioFormat);
    ~Sound();

    bool isValid() const { return m_isValid; }

    /**
     * @return Volume level in range [0..1].
     */
    float volumeLevel() const;

    /**
     * Set volume level in range [0..1].
     */
    void setVolumeLevel(float volumeLevel);

    /**
     * @return Internal buffer duration in microseconds.
     */
    qint64 playTimeElapsedUsec();

    /**
     * @return True if no data in the internal buffer.
     */
    bool isBufferUnderflow();

    /**
     * Add data to playback buffer. Audio plays immediately if it is not paused.
     */
    bool write(const quint8* data, uint size);

    /**
     * @return Audio format playing.
     */
    QnAudioFormat format() const;

    /**
     * @return Playback state.
     */
    QAudio::State state() const;

    /** Pause playing. */
    void suspend();

    /** Resume playing. */
    void resume();

    /** Clear internal audio buffer. */
    void clear();

    /**
     * @return True if audio format is supported.
     */
    static bool isFormatSupported(const QnAudioFormat& format);

private:
    uint bufferTime() const;
    bool setup();
    static int getOpenAlFormat(const QnAudioFormat& audioFormat);
    uint bitRate() const;
    uint sampleSize() const;
    bool playImpl();

    static bool outError(int err, const char *strerr);
    static int checkOpenALErrorDebug(ALCdevice *device);
    bool internalPlay(const void *data, uint size);
    void clearBuffers(bool clearAll);

    qint64 maxAudioJitterUs() const;

    /** Device specific extra delay for played audio. */
    qint64 extraAudioDelayUs() const;

private:
    mutable QnMutex m_mtx;
    QnAudioFormat m_audioFormat;

    uint m_tmpBuffer[1024];

    uint m_source;
    uint m_format;
    uint m_numChannels;
    uint m_frequency;
    uint m_bitsPerSample;
    uint m_size;
    bool m_isValid;
    ALCdevice* m_device;
    quint8* m_proxyBuffer = nullptr;
    int m_proxyBufferLen;
    bool m_deinitialized;
    bool m_paused;

    nx::ElapsedTimer m_timer;
    qint64 m_queuedDurationUs;

private:
    void internalClear();
    static int checkOpenALError(ALCdevice* device);
};

} // namespace audio
} // namespace nx
