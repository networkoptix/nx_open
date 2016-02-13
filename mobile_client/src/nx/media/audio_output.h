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

    AudioOutput();
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

    /** Maximum internal buffer size at microseconds */
    void setBufferSizeUsec(qint64 value);
    
    /** Returns maximum allowed size for the internal buffer at microseconds. */
    qint64 bufferSizeUsec() const;

    /** Returns used buffer at microseconds.
    * Free space in the buffer can be calculated as bufferSizeUsec - bufferUsedUsec.
    */
    qint64 bufferUsedUsec() const;

signals:
    void stateChanged(QAudio::State state);

private:
    QScopedPointer<AudioOutputPrivate> d_ptr;
    Q_DECLARE_PRIVATE(AudioOutput);
};

}
}
