#include "audio_output.h"
#include "abstract_audio_decoder.h"

#include <deque>
#include <atomic>

#include <nx/audio/sound.h>
#include <nx/audio/audiodevice.h>

#include <QtCore/QMutex>

namespace nx {
namespace media {

//-------------------------------------------------------------------------------------------------
// AudioOutputPrivate

struct AudioOutputPrivate
{
    AudioOutputPrivate(int initialBufferUsec, int maxBufferUsec)
    :
        bufferSizeUsec(initialBufferUsec),
        frameDurationUsec(0),
        isBuffering(false),
        initialBufferUsec(initialBufferUsec),
        maxBufferUsec(maxBufferUsec)
    {
    }

    std::unique_ptr<nx::audio::Sound> audioOutput;
    std::atomic<qint64> bufferSizeUsec; //< Current value for the buffer at microseconds
    qint64 frameDurationUsec; //< Single audio frame duration at microseconds
    std::deque<qint64> timestampQueue; //< last audio frames timestamps at UTC microseconds
    bool isBuffering; //< True if filling input buffer in progress
    const int initialBufferUsec; //< Initial size for the audio buffer
    const int maxBufferUsec; //< Maximum allowed size for the audio buffer
    mutable QMutex mutex;
};

//-------------------------------------------------------------------------------------------------
// AudioOutput

AudioOutput::AudioOutput(int initialBufferMs, int maxBufferMs)
:
    d_ptr(new AudioOutputPrivate(initialBufferMs, maxBufferMs))
{
}

AudioOutput::~AudioOutput()
{
}

void AudioOutput::suspend()
{
    Q_D(AudioOutput);
    QMutexLocker lock(&d->mutex);
    if (d->audioOutput)
        d->audioOutput->suspend();
}

void AudioOutput::resume()
{
    Q_D(AudioOutput);
    QMutexLocker lock(&d->mutex);
    if (d->audioOutput && !d->isBuffering)
        d->audioOutput->resume();
}

qint64 AudioOutput::bufferSizeUsec() const
{
    Q_D(const AudioOutput);
    return d->bufferSizeUsec;
}

void AudioOutput::write(const AudioFramePtr& audioFrame)
{
    Q_D(AudioOutput);
    QMutexLocker lock(&d->mutex);
    const bool bufferUnderflow =
        d->audioOutput && //< Audio exists
        d->audioOutput->state() == QAudio::ActiveState && //< and playing,
        d->audioOutput->isBufferUnderflow(); //< but no data in the buffer at all

    QnAudioFormat audioFormat(QnCodecAudioFormat(audioFrame->context));
    audioFormat.setCodec(lit("audio/pcm"));
    if (!d->audioOutput || //< first call
        d->audioOutput->format() != audioFormat || //< Audio format has been changed
        !d->frameDurationUsec || //< Some internal error or empty data
        bufferUnderflow) //< Audio underflow, recreate with larger buffer
    {
        // Reset audio device
        if (bufferUnderflow)
            d->bufferSizeUsec = qMin(d->bufferSizeUsec * kBufferGrowStep, kMaxLiveBufferMs * 1000);

        auto audioOutput = nx::audio::AudioDevice::instance()->createSound(audioFormat);
        d->audioOutput.reset(audioOutput);

        d->audioOutput->suspend(); //< Don't play audio while filling internal buffer.
        d->isBuffering = true;

        // Assume all frames have same duration.
        d->frameDurationUsec = audioFormat.durationForBytes(audioFrame->data.size());
        if (!d->frameDurationUsec)
            return;
    }

    d->audioOutput->write((const quint8*) audioFrame->data.data(), audioFrame->data.size());

    // Update sliding window with last audio PTS.
    d->timestampQueue.push_back(audioFrame->timestampUsec);
    
    // max amount of frames in the audio queue
    qint64 elapsed = d->audioOutput->playTimeElapsedUsec();
    int maxFrames = elapsed / d->frameDurationUsec + 1;
    
    // how many frames out of the queue now
    const int deprecatedFrames = d->timestampQueue.size() - maxFrames;
    
    if (deprecatedFrames > 0)
    {
        d->timestampQueue.erase(
            d->timestampQueue.begin(), d->timestampQueue.begin() + deprecatedFrames);
    }

    if (d->isBuffering && d->audioOutput->playTimeElapsedUsec() >= bufferSizeUsec())
    {
        d->isBuffering = false;
        d->audioOutput->resume();
    }
}

qint64 AudioOutput::currentBufferSizeUsec() const
{
    Q_D(const AudioOutput);
    QMutexLocker lock(&d->mutex);
    if (!d->audioOutput)
        return 0;
    return d->audioOutput->playTimeElapsedUsec();
}

qint64 AudioOutput::playbackPositionUsec() const
{
    Q_D(const AudioOutput);
    QMutexLocker lock(&d->mutex);
    if (!d->audioOutput || !d->frameDurationUsec || d->timestampQueue.empty())
        return kUnknownPosition; //< unknown position

    const qint64 bufferSizeUsec = d->audioOutput->playTimeElapsedUsec();

    // Consider we write a single frame and call playbackPositionUsec() immediately. In this case
    // queuedFrames returns 1. If queuedFrames is zero, then the very last queued frame already has
    // finished playing.
    const int queueSize = (int) d->timestampQueue.size();

    int queuedFrames = bufferSizeUsec / d->frameDurationUsec;
    const qint64 rest = bufferSizeUsec % d->frameDurationUsec;
    qint64 offset = 0;
    if (rest > 0)
    {
        ++queuedFrames;
        offset = d->frameDurationUsec - rest;
    }

    if (queuedFrames == 0)
    {
        // Last queued frame has finished playing.
        return d->timestampQueue[queueSize - 1] + d->frameDurationUsec;
    }

    return d->timestampQueue[qMax(0, queueSize - queuedFrames)] + offset;
}

bool AudioOutput::isBufferUnderflow() const
{
    Q_D(const AudioOutput);
    return d->audioOutput && d->audioOutput->isBufferUnderflow();
}

bool AudioOutput::isBuffering() const
{
    Q_D(const AudioOutput);
    QMutexLocker lock(&d->mutex);
    return d->isBuffering;
}

bool AudioOutput::canAcceptData() const
{
    Q_D(const AudioOutput);
    QMutexLocker lock(&d->mutex);
    if (!d->audioOutput)
        return true;
    return d->audioOutput->playTimeElapsedUsec() < kMaxArchiveBufferMs * 1000;
}

QAudio::State AudioOutput::state() const
{
    Q_D(const AudioOutput);
    QMutexLocker lock(&d->mutex);
    return d->audioOutput ? d->audioOutput->state() : QAudio::StoppedState;

}

} // namespace media
} // namespace nx
