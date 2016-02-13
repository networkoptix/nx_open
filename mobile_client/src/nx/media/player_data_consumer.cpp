#include <nx/streaming/archive_stream_reader.h>

#include "player_data_consumer.h"
#include "seamless_video_decoder.h"
#include "seamless_audio_decoder.h"
#include "abstract_audio_decoder.h"
#include "frame_metadata.h"
#include "audio_output.h"

namespace 
{
    static const int kMaxMediaQueueLen = 90;        //< max queue length for compressed data (about 3 second)
    static const int kMaxDecodedVideoQueueSize = 1; //< max queue length for decoded video which is awaiting to be rendered
}

namespace nx {
namespace media {


PlayerDataConsumer::PlayerDataConsumer(const std::unique_ptr<QnArchiveStreamReader>& archiveReader):
    QnAbstractDataConsumer(kMaxMediaQueueLen),
    m_videoDecoder(new SeamlessVideoDecoder()),
    m_audioDecoder(new SeamlessAudioDecoder()),
    m_awaitJumpCounter(0),
    m_buffering(0),
    m_hurryUpToFrame(0),
    m_noDelayState(NoDelayState::Disabled)
{
    connect(archiveReader.get(), &QnArchiveStreamReader::beforeJump,   this, &PlayerDataConsumer::onBeforeJump,   Qt::DirectConnection);
    connect(archiveReader.get(), &QnArchiveStreamReader::jumpOccured,  this, &PlayerDataConsumer::onJumpOccurred,  Qt::DirectConnection);
    connect(archiveReader.get(), &QnArchiveStreamReader::jumpCanceled, this, &PlayerDataConsumer::onJumpCanceled, Qt::DirectConnection);
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
    return base_type::canAcceptData();
}

int PlayerDataConsumer::getBufferingMask() const
{
    // todo: not implemented yet. Reserver for future use for panoramic cameras
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
        auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(m_dataQueue.atUnsafe(i));
        if (video)
        {
            minTime = std::min(minTime, video->timestamp);
            maxTime = std::max(maxTime, video->timestamp);
        }
    }
    m_dataQueue.unlock();
    return std::max(0ll, maxTime - minTime);
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

    return true; //< just ignore unknown frame type
}

bool PlayerDataConsumer::processEmptyFrame(const QnEmptyMediaDataPtr& /*data*/ )
{
    emit onEOF();
    return true;
}

bool PlayerDataConsumer::processVideoFrame(const QnCompressedVideoDataPtr& data)
{
    const bool isBofData = (data->flags & QnAbstractMediaData::MediaFlags_BOF); //< first packet after a jump
    bool displayImmediately = isBofData; //< display data immediately with no delay
    {
        QnMutexLocker lock(&m_dataProviderMutex);
        if (m_noDelayState != NoDelayState::Disabled)
            displayImmediately = true; //< display any data immediately (include intermediate frames between BOF frames) if jumps aren't processed yet
        if (m_noDelayState == NoDelayState::WaitForNextBOF && isBofData)
            m_noDelayState = NoDelayState::Disabled;
    }
    if (displayImmediately)
    {
        m_hurryUpToFrame = m_videoDecoder->currentFrameNumber();
        emit hurryUp(); //< hint to a player to avoid waiting for the currently displaying frame
    }

    QnVideoFramePtr decodedFrame;
    if (!m_videoDecoder->decode(data, &decodedFrame))
    {
        qWarning() << Q_FUNC_INFO << "Can't decode video frame. Frame is skipped.";
        return true; // false result means we want to repeat this frame latter
    }

    if (decodedFrame)
        enqueueVideoFrame(std::move(decodedFrame));

    return true;
}

void PlayerDataConsumer::enqueueVideoFrame(QnVideoFramePtr decodedFrame)
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

QnVideoFramePtr PlayerDataConsumer::dequeueVideoFrame()
{
    QnMutexLocker lock(&m_queueMutex);
    if (m_decodedVideo.empty())
        return QnVideoFramePtr(); //< no data
    QnVideoFramePtr result = std::move(m_decodedVideo.front());
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

bool PlayerDataConsumer::processAudioFrame(const QnCompressedAudioDataPtr& data )
{
    QnAudioFramePtr decodedFrame;
    if (!m_audioDecoder->decode(data, &decodedFrame))
    {
        qWarning() << Q_FUNC_INFO << "Can't decode audio frame. Frame is skipped.";
        return true; // false result means we want to repeat this frame latter
    }

    if (!decodedFrame || !decodedFrame->context)
        return true; //< just skip frame
    
    m_audioOutput->write(decodedFrame);
    return true;
}

void PlayerDataConsumer::onBeforeJump(qint64 /* timeUsec */)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    QnMutexLocker lock(&m_dataProviderMutex);
    ++m_awaitJumpCounter;
    m_buffering = getBufferingMask();

    /*
    * The purpose of this variable is prevent doing delay between frames while they are displayed
    * We supposed to decode/display they at maximum speed unless lat jump command will be processed
    */
    m_noDelayState = NoDelayState::Activated;
}

void PlayerDataConsumer::onJumpCanceled(qint64 /*timeUsec*/)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    // Previous jump command hasn't been executed due to new jump command received
    QnMutexLocker lock(&m_dataProviderMutex);
    --m_awaitJumpCounter;
    Q_ASSERT(m_awaitJumpCounter >= 0);
}

void PlayerDataConsumer::onJumpOccurred(qint64 /* timeUsec */)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    clearUnprocessedData(); //< Clear input (undecoded) data queue
    QnMutexLocker lock(&m_dataProviderMutex);
    if (--m_awaitJumpCounter == 0)
    {
        /*
        * This function is called from dataProvider thread. PlayerConsumer may still process previous frame
        * So, leave noDelay state a bit later, when next BOF frame will be received
        */
        m_noDelayState = NoDelayState::WaitForNextBOF;
    }
}

} // namespace media
} // namespace nx
