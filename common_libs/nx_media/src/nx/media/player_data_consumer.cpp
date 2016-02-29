#include "player_data_consumer.h"

#include <nx/streaming/archive_stream_reader.h>

#include "seamless_video_decoder.h"
#include "seamless_audio_decoder.h"
#include "abstract_audio_decoder.h"
#include "frame_metadata.h"
#include "audio_output.h"

namespace nx {
namespace media {

namespace {

// Max queue length for compressed data (about 3 second).
static const int kMaxMediaQueueLen = 90;

// Max queue length for decoded video which is awaiting to be rendered.
static const int kMaxDecodedVideoQueueSize = 1;

} // namespace

PlayerDataConsumer::PlayerDataConsumer(
    const std::unique_ptr<QnArchiveStreamReader>& archiveReader)
:
    QnAbstractDataConsumer(kMaxMediaQueueLen),
    m_videoDecoder(new SeamlessVideoDecoder()),
    m_audioDecoder(new SeamlessAudioDecoder()),
    m_awaitJumpCounter(0),
    m_buffering(0),
    m_hurryUpToFrame(0),
    m_noDelayState(NoDelayState::Disabled)
{
    connect(archiveReader.get(), &QnArchiveStreamReader::beforeJump,
        this, &PlayerDataConsumer::onBeforeJump, Qt::DirectConnection);
    connect(archiveReader.get(), &QnArchiveStreamReader::jumpOccured,
        this, &PlayerDataConsumer::onJumpOccurred, Qt::DirectConnection);
    connect(archiveReader.get(), &QnArchiveStreamReader::jumpCanceled,
        this, &PlayerDataConsumer::onJumpCanceled, Qt::DirectConnection);
}

PlayerDataConsumer::~PlayerDataConsumer()
{
    stop();
}

void PlayerDataConsumer::pleaseStop()
{
    base_type::pleaseStop();
    m_videoDecoder->pleaseStop();
    m_audioDecoder->pleaseStop();
    m_queueWaitCond.wakeAll();
}

bool PlayerDataConsumer::canAcceptData() const
{
    if (m_audioOutput && !m_audioOutput->canAcceptData())
        return false;
    else
        return base_type::canAcceptData();
}

int PlayerDataConsumer::getBufferingMask() const
{
    // TODO: Not implemented yet. Reserved for future use for panoramic cameras.
    return 1;
}

void PlayerDataConsumer::putData(const QnAbstractDataPacketPtr& data)
{
    base_type::putData(data);
}

qint64 PlayerDataConsumer::queueVideoDurationUsec() const
{
    qint64 minTime = std::numeric_limits<qint64>::max();
    qint64 maxTime = 0;
    m_dataQueue.lock();
    for (int i = 0; i < m_dataQueue.size(); ++i)
    {
        auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(
            m_dataQueue.atUnsafe(i));
        if (video)
        {
            minTime = std::min(minTime, video->timestamp);
            maxTime = std::max(maxTime, video->timestamp);
        }
    }
    m_dataQueue.unlock();
    return std::max(0ll, maxTime - minTime);
}

const AudioOutput* PlayerDataConsumer::audioOutput() const
{
    return m_audioOutput.get();
}

bool PlayerDataConsumer::processData(const QnAbstractDataPacketPtr& data)
{
    auto emptyFrame = std::dynamic_pointer_cast<QnEmptyMediaData>(data);
    if (emptyFrame)
        return processEmptyFrame(emptyFrame);

    auto videoFrame = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
    if (videoFrame)
        return processVideoFrame(videoFrame);

    auto audioFrame = std::dynamic_pointer_cast<QnCompressedAudioData>(data);
    if (audioFrame)
        return processAudioFrame(audioFrame);

    return true; //< Just ignore unknown frame type.
}

bool PlayerDataConsumer::processEmptyFrame(const QnEmptyMediaDataPtr& /*data*/)
{
    emit onEOF();
    return true;
}

QnCompressedVideoDataPtr PlayerDataConsumer::queueVideoFrame(
    const QnCompressedVideoDataPtr& videoFrame)
{
    if (!m_audioOutput)
        return videoFrame; //< Pre-decoding queue is not required.

    m_predecodeQueue.push_back(videoFrame);

    QnCompressedVideoDataPtr result = m_predecodeQueue.front();
    if (result->timestamp < m_audioOutput->playbackPositionUsec())
    {
        // Frame time is earlier than the audio buffer, no need to delay it anyway.
        m_predecodeQueue.pop_front();
        return result;
    }

    if (m_audioOutput->isBuffering())
    {
        // Frame time is inside the audio buffer, delay the frame while audio is buffered.
        return QnCompressedVideoDataPtr();
    }
    m_predecodeQueue.pop_front();
    return result;
}

bool PlayerDataConsumer::processVideoFrame(const QnCompressedVideoDataPtr& videoFrame)
{
    QnCompressedVideoDataPtr data = queueVideoFrame(videoFrame);
    if (!data)
        return true; //< Frame is processed.

    // First packet after a jump.
    const bool isBofData = (data->flags & QnAbstractMediaData::MediaFlags_BOF);

    bool displayImmediately = isBofData; //< display data immediately with no delay
    {
        QnMutexLocker lock(&m_dataProviderMutex);
        if (m_noDelayState != NoDelayState::Disabled)
        {
            // Display any data immediately (include intermediate frames between BOF frames) if the
            // jumps are not processed yet.
            displayImmediately = true;
        }
        if (m_noDelayState == NoDelayState::WaitForNextBOF && isBofData)
            m_noDelayState = NoDelayState::Disabled;
    }
    if (displayImmediately)
    {
        m_hurryUpToFrame = m_videoDecoder->currentFrameNumber();
        emit hurryUp(); //< Hint to a player to avoid waiting for the currently displaying frame.
    }

    QVideoFramePtr decodedFrame;
    if (!m_videoDecoder->decode(data, &decodedFrame))
    {
        qWarning() << Q_FUNC_INFO << "Can't decode video frame. Frame is skipped.";
        return true; //False result means we want to repeat this frame later.
    }

    if (decodedFrame)
        enqueueVideoFrame(std::move(decodedFrame));

    return true;
}

void PlayerDataConsumer::enqueueVideoFrame(QVideoFramePtr decodedFrame)
{
    Q_ASSERT(decodedFrame);
    QnMutexLocker lock(&m_queueMutex);
    while (m_decodedVideo.size() >= kMaxDecodedVideoQueueSize && !needToStop())
        m_queueWaitCond.wait(&m_queueMutex);
    if (needToStop())
        return;
    m_decodedVideo.push_back(std::move(decodedFrame));
    lock.unlock();
    emit gotVideoFrame();
}

QVideoFramePtr PlayerDataConsumer::dequeueVideoFrame()
{
    QnMutexLocker lock(&m_queueMutex);
    if (m_decodedVideo.empty())
        return QVideoFramePtr(); //< no data
    QVideoFramePtr result = std::move(m_decodedVideo.front());
    m_decodedVideo.pop_front();
    lock.unlock();

    FrameMetadata metadata = FrameMetadata::deserialize(result);
    if (metadata.frameNum <= m_hurryUpToFrame)
    {
        metadata.noDelay = true;
        metadata.serialize(result);
    }

    m_queueWaitCond.wakeAll();
    return result;
}

bool PlayerDataConsumer::processAudioFrame(const QnCompressedAudioDataPtr& data)
{
    AudioFramePtr decodedFrame;
    if (!m_audioDecoder->decode(data, &decodedFrame))
    {
        qWarning() << Q_FUNC_INFO << "Can't decode audio frame. Frame is skipped.";
        return true; // False result means we want to repeat this frame later.
    }

    if (!decodedFrame || !decodedFrame->context)
        return true; //< decoder is buffering. true means input frame processed

    if (!m_audioOutput)
        m_audioOutput.reset(new AudioOutput(kInitialBufferMs * 1000, kMaxLiveBufferMs * 1000));
    m_audioOutput->write(decodedFrame);
    return true;
}

void PlayerDataConsumer::onBeforeJump(qint64 /*timeUsec*/)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    QnMutexLocker lock(&m_dataProviderMutex);
    ++m_awaitJumpCounter;
    m_buffering = getBufferingMask();

    // The purpose of this variable is prevent doing delay between frames while they are displayed.
    // We supposed to decode/display them at maximum speed unless the last jump command is
    // processed.
    m_noDelayState = NoDelayState::Activated;
}

void PlayerDataConsumer::onJumpCanceled(qint64 /*timeUsec*/)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    // Previous jump command has not been executed due to the new jump command received.
    QnMutexLocker lock(&m_dataProviderMutex);
    --m_awaitJumpCounter;
    Q_ASSERT(m_awaitJumpCounter >= 0);
}

void PlayerDataConsumer::onJumpOccurred(qint64 /*timeUsec*/)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    clearUnprocessedData(); //< Clear input (undecoded) data queue.

    QnMutexLocker lock(&m_dataProviderMutex);

    --m_awaitJumpCounter;
    if (m_awaitJumpCounter == 0)
    {
        // This function is called from dataProvider thread. PlayerConsumer may still process the
        // previous frame. So, leave noDelay state a bit later, when the next BOF frame will be
        // received.
        m_noDelayState = NoDelayState::WaitForNextBOF;
    }
}

} // namespace media
} // namespace nx
