#pragma once

#include <QtMultimedia/QAudioOutput>
#include <nx/streaming/audio_data_packet.h>

#include "media_fwd.h"

namespace nx {
namespace media {

/*
* This class extends QT audio output and provides more flexible methods for time control. They are used for audio/video sync
*/
class AudioOutputPrivate;
class AudioOutput: public QAudioOutput
{
    Q_OBJECT
public:
    
    static const qint64 kUnknownPosition = -1;
    
    /*
    * \param initialBufferUsec initial media buffer size.
    * \param maxBufferUsec media buffer grows automatically in range [initialBufferUsec..maxBufferUsec] if underflow occurred
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

    /** Add audio packet to the internal audio buffer */
    void write(const QnAudioFramePtr& audioFrame);

    /** Current UTC playback position at microseconds (what you hear now) */
    qint64 playbackPositionUsec() const;

    /*
    * Returns size for the internal buffer at microseconds. 
    * This value could be increased automatically if AudioOutput detect buffer underflow issue.
    */
    qint64 maxBufferSizeUsec() const;

    /** Returns used buffer size at microseconds.
    * Free space in the buffer can be calculated as maxBufferSizeUsec - currentBufferSizeUsec.
    */
    qint64 currentBufferSizeUsec() const;

    /*
    * Returns true if audio buffer is too small and need initial buffering.
    * Initial buffering fills half of the audio buffer before start playing it.
    * If audio buffer is underflow during playback AudioOutput starts buffering again.
    */
    bool isBuffering() const;

signals:
    void stateChanged(QAudio::State state);

private:
    QScopedPointer<AudioOutputPrivate> d_ptr;
    Q_DECLARE_PRIVATE(AudioOutput);
};

}
}
