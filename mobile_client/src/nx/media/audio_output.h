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
class AudioOutput: public QAudioOutput
{
    Q_OBJECT

public:

    static const qint64 kUnknownPosition = -1;
    
    /**
     * @param initialBufferUsec Initial media buffer size.
     * @param maxBufferUsec Media buffer grows automatically in range
     * [initialBufferUsec..maxBufferUsec] if underflow occurs.
     */
    AudioOutput(int initialBufferUsec, int maxBufferUsec);

    virtual ~AudioOutput();
            
    /** Start play audio */
    void start();

    /** Stop play audio */
    void stop();

    /** Pause play audio */
    void suspend();

    /** Resume playing */
    void resume();

    /** Add audio packet to the internal audio buffer. */
    void write(const QnAudioFramePtr& audioFrame);

    /**
     * @return Current UTC playback position in microseconds (what you hear now).
     */
    qint64 playbackPositionUsec() const;

    /**
     * @return Size for the internal buffer in microseconds. This value could be increased
     * automatically if AudioOutput detects buffer underflow issue.
     */
    qint64 maxBufferSizeUsec() const;

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

signals:
    void stateChanged(QAudio::State state);

private:
    QScopedPointer<AudioOutputPrivate> d_ptr;
    Q_DECLARE_PRIVATE(AudioOutput);
};

} // namespace media
} // namespace nx
