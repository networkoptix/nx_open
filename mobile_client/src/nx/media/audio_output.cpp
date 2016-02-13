#include "audio_output.h"
#include "abstract_audio_decoder.h"

#include <deque>

namespace nx {
namespace media {

// ---------------------------------- AudioOutputPrivate --------------------------------

struct AudioOutputPrivate
{
    AudioOutputPrivate() :
        bufferSizeUsec(0),
        audioIoDevice(nullptr),
        frameDurationUsec(0)
    {
    }

    std::unique_ptr<QAudioOutput> audioOutput;
    qint64 bufferSizeUsec;                  //< User defined audio buffer at microseconds
    QIODevice* audioIoDevice;               //< QAudioOutput internal IO device
    qint64 frameDurationUsec;               //< Single audio frame duration at microseconds
    std::deque<qint64> timestampQueue;      //< last audio frames timestamps at UTC microseconds
};

// ---------------------------------- AudioOutput --------------------------------

AudioOutput::AudioOutput():
    QAudioOutput(),
    d_ptr(new AudioOutputPrivate())

{

}

AudioOutput::~AudioOutput()
{

}

void AudioOutput::start()
{
    Q_D(AudioOutput);
    if (d->audioOutput)
        d->audioOutput->start();
}

void AudioOutput::stop()
{
    Q_D(AudioOutput);
    if (d->audioOutput)
        d->audioOutput->stop();
}

void AudioOutput::suspend()
{
    Q_D(AudioOutput);
    if (d->audioOutput)
        d->audioOutput->suspend();
}

void AudioOutput::resume()
{
    Q_D(AudioOutput);
    if (d->audioOutput)
        d->audioOutput->resume();
}

void AudioOutput::setBufferSizeUsec(qint64 value)
{
    Q_D(AudioOutput);
    d->bufferSizeUsec = value;
}

qint64 AudioOutput::bufferSizeUsec() const
{
    Q_D(const AudioOutput);
    return d->bufferSizeUsec;
}

void AudioOutput::write(const QnAudioFramePtr& audioFrame)
{
    Q_D(AudioOutput);

    QnCodecAudioFormat audioFormat(audioFrame->context);
    audioFormat.setCodec(lit("audio/pcm"));
    if (!d->audioOutput || d->audioOutput->format() != audioFormat || !d->audioIoDevice || !d->frameDurationUsec)
    {
        d->audioOutput.reset(new QAudioOutput(audioFormat));
        d->audioOutput->setBufferSize(audioFormat.bytesForDuration(d->bufferSizeUsec));
        d->audioIoDevice = d->audioOutput->start();
        if (!d->audioIoDevice)
            return; //< some error ocured. Wait for next frame

        //Assume all frames have same duration
        d->frameDurationUsec = audioFormat.durationForBytes(audioFrame->data.size());
        if (!d->frameDurationUsec)
            return;
    }

    d->audioIoDevice->write((const char *)audioFrame->data.data(), audioFrame->data.size());

    // Update sliding window with last audio PTS
    d->timestampQueue.push_back(audioFrame->timestampUsec);
    const int maxFrames = d->bufferSizeUsec / d->frameDurationUsec;     //< max amount of frames in the audio queue
    const int deprecatedFrames = d->timestampQueue.size() - maxFrames;  //< how many frames out of the queue now
    if (deprecatedFrames > 0)
        d->timestampQueue.erase(d->timestampQueue.begin(), d->timestampQueue.begin() + deprecatedFrames);
}

qint64 AudioOutput::bufferUsedUsec() const
{
    Q_D(const AudioOutput);
    if (!d->audioOutput)
        return 0;
    const int bufferUsedBytes = d->audioOutput->bufferSize() - d->audioOutput->bytesFree();
    return d->audioOutput->format().durationForBytes(bufferUsedBytes);
}

qint64 AudioOutput::playbackPositionUsec() const
{
    Q_D(const AudioOutput);
    if (!d->frameDurationUsec || d->timestampQueue.empty())
        return kUnknownPosition; //< unknown position
    // Consider we write a single frame and call playbackPositionUsec() immidiatly. At this case queuedFrames returns 1.
    // If queuedFrames is zero then the very last queued frame already has finished playing.
    const int queueSize = (int) d->timestampQueue.size();
    const int queuedFrames = bufferUsedUsec() / d->frameDurationUsec;
    if (queuedFrames == 0)
        return d->timestampQueue[queueSize - 1] + d->frameDurationUsec; //< last queued frame has finished playing
    return d->timestampQueue[qMax(0, queueSize - queuedFrames)];
}

}
}
