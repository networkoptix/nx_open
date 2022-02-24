// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_output.h"
#include "abstract_audio_decoder.h"

#include <deque>
#include <atomic>

#include <QtCore/QMutex>

#include <nx/audio/sound.h>
#include <nx/audio/audiodevice.h>
#include <nx/audio/processor.h>

#include <utils/math/math.h>

#include <nx/utils/log/log.h>

namespace nx {
namespace media {

using namespace nx::audio;

using AudioFilter = std::function<Format(QnByteArray&, Format)>;

namespace {

Format getCompatibleFormat(Format format, QList<AudioFilter>* outFilters)
{
    outFilters->clear();

    if (Sound::isFormatSupported(format))
        return format;

    if (format.sampleType == Format::SampleType::floatingPoint)
    {
        format.sampleType = Format::SampleType::signedInt;
        if (Sound::isFormatSupported(format))
        {
            outFilters->push_back(Processor::float2int32);
            return format;
        }
        format.sampleSize = 16;
        outFilters->push_back(Processor::float2int16);
        if (Sound::isFormatSupported(format))
            return format;
    }
    else if (format.sampleSize == 32)
    {
        format.sampleSize = 16;
        outFilters->push_back(Processor::int32Toint16);
        if (Sound::isFormatSupported(format))
            return format;
    }

    format.channelCount = qMin(format.channelCount, 2);
    outFilters->push_back(Processor::downmix);
    if (Sound::isFormatSupported(format))
        return format;

    outFilters->clear();
    return Format(); //< conversion rule not found
}

} // namespace

//-------------------------------------------------------------------------------------------------
// AudioOutputPrivate

class AudioOutputPrivate
{
public:
    AudioOutputPrivate(int initialBufferUsec, int maxBufferUsec):
        bufferSizeUsec(initialBufferUsec),
        frameDurationUsec(0),
        bufferingBytes(0),
        initialBufferUsec(initialBufferUsec),
        maxBufferUsec(maxBufferUsec)
    {
    }

    std::unique_ptr<Sound> audioOutput;
    std::atomic<qint64> bufferSizeUsec; //< Current value for the buffer at microseconds
    qint64 frameDurationUsec; //< Single audio frame duration at microseconds
    std::deque<qint64> timestampQueue; //< last audio frames timestamps at UTC microseconds
    int bufferingBytes; //< Value > 0 if filling input buffer in progress
    const int initialBufferUsec; //< Initial size for the audio buffer
    const int maxBufferUsec; //< Maximum allowed size for the audio buffer
    mutable QMutex mutex;

    Format lastAudioFormat;
    Format warnedAudioFormat; //< using for log messages
    QList<AudioFilter> audioFilters;
};

//-------------------------------------------------------------------------------------------------
// AudioOutput

AudioOutput::AudioOutput(int initialBufferMs, int maxBufferMs):
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
    if (d->audioOutput && d->bufferingBytes <= 0)
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

    Format codecAudioFormat = nx::audio::formatFromMediaContext(audioFrame->context);
    Format audioFormat(codecAudioFormat);
    audioFormat.codec = "audio/pcm";
    if (!d->audioOutput || //< first call
        d->lastAudioFormat != audioFormat || //< Audio format has been changed
        !d->frameDurationUsec || //< Some internal error or empty data
        bufferUnderflow) //< Audio underflow, recreate with larger buffer
    {
        // Reset audio device
        if (bufferUnderflow)
            d->bufferSizeUsec = qMin(d->bufferSizeUsec * kBufferGrowStep, kMaxLiveBufferMs * 1000);

        d->lastAudioFormat = audioFormat;

        auto compatibleFormat = getCompatibleFormat(audioFormat, &d->audioFilters);
        auto audioOutput = AudioDevice::instance()->createSound(compatibleFormat);
        if (audioOutput == nullptr)
        {
            if (d->warnedAudioFormat != audioFormat)
            {
                d->warnedAudioFormat = audioFormat;
                NX_WARNING(this, "Cannot create compatible audio output for %1", audioFormat);
            }
            return;
        }
        d->audioOutput.reset(audioOutput);

        d->audioOutput->suspend(); //< Don't play audio while filling internal buffer.
        d->bufferingBytes = audioFormat.bytesForDuration(
            std::chrono::microseconds(d->bufferSizeUsec));

        // Assume all frames have same duration.
        d->frameDurationUsec = audioFormat.durationForBytes(audioFrame->data.size()).count();
        if (!d->frameDurationUsec)
            return;
    }

    for (const auto& filter: d->audioFilters)
        codecAudioFormat = filter(audioFrame->data, codecAudioFormat);

    d->audioOutput->write((const quint8*) audioFrame->data.data(), audioFrame->data.size());

    // Update sliding window with last audio PTS.
    d->timestampQueue.push_back(audioFrame->timestampUsec);

    // max amount of frames in the audio queue
    qint64 elapsed = d->audioOutput->playTimeElapsedUsec();
    int maxFrames = elapsed / d->frameDurationUsec + 1;

    // how many frames out of the queue now
    const int deprecatedFrames = (int) d->timestampQueue.size() - maxFrames;

    if (deprecatedFrames > 0)
    {
        d->timestampQueue.erase(
            d->timestampQueue.begin(), d->timestampQueue.begin() + deprecatedFrames);
    }

    if (d->bufferingBytes > 0)
    {
        d->bufferingBytes = qMax(d->bufferingBytes - (int) audioFrame->data.size(), 0);
        if (d->bufferingBytes <= 0)
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

    qint64 bufferSizeUsec = d->audioOutput->playTimeElapsedUsec();

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

    qint64 resultUs = (queuedFrames == 0) ?
        d->timestampQueue[queueSize - 1] + d->frameDurationUsec :
        d->timestampQueue[qMax(0, queueSize - queuedFrames)] + offset;

    return resultUs;
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
    return d->bufferingBytes > 0;
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
