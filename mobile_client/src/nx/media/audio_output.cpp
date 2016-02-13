#include "audio_output.h"
#include "abstract_audio_decoder.h"

#include <deque>

namespace nx {
namespace media {

// ---------------------------------- AudioOutputPrivate --------------------------------

struct AudioOutputPrivate
{
    AudioOutputPrivate(int initialBufferUsec, int maxBufferUsec):
        bufferSizeUsec(initialBufferUsec),
        audioIoDevice(nullptr),
        frameDurationUsec(0),
        isBuffering(false),
        initialBufferUsec(initialBufferUsec),
        maxBufferUsec(maxBufferUsec)
    {
    }

    std::unique_ptr<QAudioOutput> audioOutput;
    qint64 bufferSizeUsec;                  //< Current value for the buffer at microseconds
    QIODevice* audioIoDevice;               //< QAudioOutput internal IO device
    qint64 frameDurationUsec;               //< Single audio frame duration at microseconds
    std::deque<qint64> timestampQueue;      //< last audio frames timestamps at UTC microseconds
    bool isBuffering;                       //< True if filling input buffer in progress
    const int initialBufferUsec;            //< Initial size for the audio buffer
    const int maxBufferUsec;                //< Maximum allowed size for the audio buffer
};

// ---------------------------------- AudioOutput --------------------------------

AudioOutput::AudioOutput(int initialBufferMs, int maxBufferMs):
    QAudioOutput(),
    d_ptr(new AudioOutputPrivate(initialBufferMs, maxBufferMs))
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
    if (d->audioOutput && !d->isBuffering)
        d->audioOutput->resume();
}

qint64 AudioOutput::maxBufferSizeUsec() const
{
    Q_D(const AudioOutput);
    return d->bufferSizeUsec;
}

void AudioOutput::write(const QnAudioFramePtr& audioFrame)
{
    Q_D(AudioOutput);

    const bool bufferUnderflow = 
        d->audioOutput &&                                             //< Audio exists
        d->audioOutput->state() == QAudio::ActiveState &&             //< and playing,
        d->audioOutput->bytesFree() == d->audioOutput->bufferSize();  //< but no data in the buffer at all

    QnCodecAudioFormat audioFormat(audioFrame->context);
    audioFormat.setCodec(lit("audio/pcm"));
    if (!d->audioOutput ||                           //< first call
         d->audioOutput->format() != audioFormat ||  //< Audio format has been changed
        !d->audioIoDevice ||                         //< Some internal error
        !d->frameDurationUsec ||                     //< Some internal error or empty data
        bufferUnderflow)                             //< Audio underflow, recreate with bigger buffer
    {
        // Reset audio device
        if (bufferUnderflow)
            d->bufferSizeUsec = qMin(d->bufferSizeUsec * kBufferGrowStep, kMaxBufferMs * 1000);

        d->audioOutput.reset(new QAudioOutput(audioFormat));
        d->audioOutput->setBufferSize(audioFormat.bytesForDuration(d->bufferSizeUsec));
        d->audioIoDevice = d->audioOutput->start();
        if (!d->audioIoDevice)
            return; //< some error occurred. Wait for the next frame

        d->audioOutput->suspend(); //< Don't play audio while filling internal buffer
        d->isBuffering = true;

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

    if (d->isBuffering && currentBufferSizeUsec() >= maxBufferSizeUsec() / 2)
    {
        d->isBuffering = false;
        d->audioOutput->resume();
    }

}

qint64 AudioOutput::currentBufferSizeUsec() const
{
    Q_D(const AudioOutput);
    if (!d->audioOutput)
        return 0;
    int bufferUsedBytes = d->audioOutput->bufferSize() - d->audioOutput->bytesFree();
    if (d->audioOutput->state() != QAudio::ActiveState)
        bufferUsedBytes += d->audioIoDevice->bytesAvailable(); //< Addition buffer. data is queued but don't processed.
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
    const int queuedFrames = currentBufferSizeUsec() / d->frameDurationUsec;
    if (queuedFrames == 0)
        return d->timestampQueue[queueSize - 1] + d->frameDurationUsec; //< last queued frame has finished playing
    return d->timestampQueue[qMax(0, queueSize - queuedFrames)];
}

bool AudioOutput::isBuffering() const
{
    Q_D(const AudioOutput);
    return d->isBuffering;
}

}
}
