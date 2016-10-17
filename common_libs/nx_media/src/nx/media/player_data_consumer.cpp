#include "player_data_consumer.h"

#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/debug_utils.h>

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
static const int kMaxDecodedVideoQueueSize = 2;

QSize qMax(const QSize& size1, const QSize& size2)
{
    return size1.height() > size2.height() ? size1 : size2;
}

} // namespace

PlayerDataConsumer::PlayerDataConsumer(
    const std::unique_ptr<QnArchiveStreamReader>& archiveReader)
:
    QnAbstractDataConsumer(kMaxMediaQueueLen),
    m_awaitJumpCounter(0),
    m_buffering(0),
    m_noDelayState(NoDelayState::Disabled),
    m_lastFrameTimeUs(AV_NOPTS_VALUE),
    m_lastDisplayedTimeUs(AV_NOPTS_VALUE)
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
    QnMutexLocker lock(&m_decoderMutex);
    for (auto& videoDecoder: m_videoDecoders)
        videoDecoder->pleaseStop();
    if (m_audioDecoder)
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
    auto unsafeQueue = m_dataQueue.lock();
    for (int i = 0; i < unsafeQueue.size(); ++i)
    {
        auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(
            unsafeQueue.at(i));
        if (video)
        {
            minTime = std::min(minTime, video->timestamp);
            maxTime = std::max(maxTime, video->timestamp);
        }
    }
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
    int videoChannel = videoFrame->channelNumber;
    auto archiveReader = dynamic_cast<const QnArchiveStreamReader*>(videoFrame->dataProvider);
    if (archiveReader)
    {
        auto videoLayout = archiveReader->getDPVideoLayout();
        if (videoLayout)
            m_awaitingFramesMask.setChannelCount(videoLayout->channelCount());
    }

    {
        QnMutexLocker lock(&m_decoderMutex);
        while (m_videoDecoders.size() <= videoChannel)
        {
            auto videoDecoder = new SeamlessVideoDecoder();
            videoDecoder->setVideoGeometryAccessor(m_videoGeometryAccessor);
            m_videoDecoders.push_back(SeamlessVideoDecoderPtr(videoDecoder));
        }
    }
    SeamlessVideoDecoder* videoDecoder = m_videoDecoders[videoChannel].get();

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
        m_hurryUpToFrame.videoChannel = videoChannel;
        m_hurryUpToFrame.frameNumber = videoDecoder->currentFrameNumber();
        emit hurryUp(); //< Hint to a player to avoid waiting for the currently displaying frame.
    }

    QVideoFramePtr decodedFrame;
    if (!videoDecoder->decode(data, &decodedFrame))
    {
        qWarning() << Q_FUNC_INFO << "Can't decode video frame. Frame is skipped.";
        // False result means we want to repeat this frame later, thus, returning true.
    }
    else
    {
        if (decodedFrame)
            enqueueVideoFrame(std::move(decodedFrame));
    }

    return true;
}

void PlayerDataConsumer::enqueueVideoFrame(QVideoFramePtr decodedFrame)
{
    NX_ASSERT(decodedFrame);
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

    /**
     * m_hurryUpToFrame hold frame number from which player should display data without delay.
     * Additionally, for panoramic cameras frames should be processed without delay unless
     * it got at least 1 frame for each channel
     */
    if ((metadata.videoChannel != m_hurryUpToFrame.videoChannel &&
        m_awaitingFramesMask.hasChannel(m_hurryUpToFrame.videoChannel)) ||
        metadata.frameNum < m_hurryUpToFrame.frameNumber)
    {
        metadata.noDelay = true;
    }
    else
    {
        if (!m_awaitingFramesMask.isEmpty())
            metadata.noDelay = true; //< include noDelay to the first displayed frame
        m_awaitingFramesMask.removeChannel(metadata.videoChannel);
    }
    if (metadata.noDelay)
        metadata.serialize(result);

    m_queueWaitCond.wakeAll();

    if (result)
        m_lastFrameTimeUs = result->startTime() * 1000;
    return result;
}

bool PlayerDataConsumer::processAudioFrame(const QnCompressedAudioDataPtr& data)
{
    {
        QnMutexLocker lock(&m_decoderMutex);
        if (!m_audioDecoder)
            m_audioDecoder.reset(new SeamlessAudioDecoder());
    }

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

void PlayerDataConsumer::onBeforeJump(qint64 timeUsec)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    QnMutexLocker lock(&m_dataProviderMutex);
    ++m_awaitJumpCounter;
    m_buffering = getBufferingMask();

    // The purpose of this variable is prevent doing delay between frames while they are displayed.
    // We supposed to decode/display them at maximum speed unless the last jump command is
    // processed.
    m_noDelayState = NoDelayState::Activated;
    m_awaitingFramesMask.setMask();
    m_lastDisplayedTimeUs = m_lastFrameTimeUs = timeUsec; //< force position to the new place
}

void PlayerDataConsumer::onJumpCanceled(qint64 /*timeUsec*/)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    // Previous jump command has not been executed due to the new jump command received.
    QnMutexLocker lock(&m_dataProviderMutex);
    --m_awaitJumpCounter;
    NX_ASSERT(m_awaitJumpCounter >= 0);
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

void PlayerDataConsumer::endOfRun()
{
    QnMutexLocker lock(&m_decoderMutex);
    m_videoDecoders.clear();
    m_audioDecoder.reset();
}

QSize PlayerDataConsumer::currentResolution() const
{
    QSize result;
    for (const auto& decoder: m_videoDecoders)
        result = qMax(result, decoder->currentResolution());

    return result;
}

AVCodecID PlayerDataConsumer::currentCodec() const
{
    for (const auto& videoDecoder: m_videoDecoders)
    {
        auto result = videoDecoder->currentCodec();
        if (result != AV_CODEC_ID_NONE)
            return result;
    }
    return AV_CODEC_ID_NONE;
}

void PlayerDataConsumer::setVideoGeometryAccessor(VideoGeometryAccessor videoGeometryAccessor)
{
    NX_ASSERT(videoGeometryAccessor);
    m_videoGeometryAccessor = videoGeometryAccessor;
}

qint64 PlayerDataConsumer::getCurrentTime() const
{
    return m_lastFrameTimeUs;
}

qint64 PlayerDataConsumer::getDisplayedTime() const
{
    return m_lastDisplayedTimeUs;
}

void PlayerDataConsumer::setDisplayedTimeUs(qint64 value)
{
    m_lastDisplayedTimeUs = value;
}

qint64 PlayerDataConsumer::getNextTime() const
{
    return AV_NOPTS_VALUE;
}

qint64 PlayerDataConsumer::getExternalTime() const
{
    return m_lastDisplayedTimeUs;
}

} // namespace media
} // namespace nx
