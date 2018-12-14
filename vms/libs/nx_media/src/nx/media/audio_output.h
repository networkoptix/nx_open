#pragma once

#include <QtMultimedia/QAudioOutput>
#include <nx/streaming/audio_data_packet.h>

#include "media_fwd.h"

namespace nx {
namespace media {

class AudioOutputPrivate;

/* Extends QT audio output and provides more flexible methods for time control. They are used for
 * audio/video sync.
 */
class AudioOutput
{
public:

    static const qint64 kUnknownPosition = -1;

    /**
     * @param initialBufferUsec Initial media buffer size.
     * @param maxBufferUsec Media buffer grows automatically in range
     * [initialBufferUsec..maxBufferUsec] if underflow occurs.
     */
    AudioOutput(int initialBufferUsec, int maxBufferUsec);

    virtual ~AudioOutput();

    /** Pause play audio */
    void suspend();

    /** Resume playing */
    void resume();

    /** Add audio packet to the internal audio buffer. */
    void write(const AudioFramePtr& audioFrame);

    /**
     * @return Current UTC playback position in microseconds (what you hear now).
     */
    qint64 playbackPositionUsec() const;

    /**
     * @return True in internal audio buffer is underflow
     */
    bool isBufferUnderflow() const;

    /**
     * @return Size for the internal buffer in microseconds. This value could be increased
     * automatically if AudioOutput detects buffer underflow issue.
     */
    qint64 bufferSizeUsec() const;

    /**
     * Free space in the buffer can be calculated as maxBufferSizeUsec() - currentBufferSizeUsec().
     * @return Used buffer size in microseconds.
     */
    qint64 currentBufferSizeUsec() const;

    /**
     * Initial buffering fills half of the audio buffer before start playing it. If audio buffer is
     * underflow during playback AudioOutput starts buffering again.
     * @return True if audio buffer is too small and need initial buffering.
     */
    bool isBuffering() const;

    /**
     * @return False if audio buffer is full and can't accept new data right now
     */
    bool canAcceptData() const;

    QAudio::State state() const;

private:
    QScopedPointer<AudioOutputPrivate> d_ptr;
    Q_DECLARE_PRIVATE(AudioOutput);
};
typedef std::shared_ptr<AudioOutput> AudioOutputPtr;
typedef std::shared_ptr<const AudioOutput> ConstAudioOutputPtr;

} // namespace media
} // namespace nx
