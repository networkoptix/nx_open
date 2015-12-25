#include "player_data_consumer.h"
#include "seamless_video_decoder.h"
#include "frame_metadata.h"

#include <nx/streaming/archive_stream_reader.h>

namespace 
{
	static const int kLiveMediaQueueLen = 20;       //< default media queue line
	static const int kMaxDecodedVideoQueueSize = 2; //< max queue len for decoded video awaiting to be rendered
}

namespace nx
{
namespace media
{

PlayerDataConsumer::PlayerDataConsumer(const std::unique_ptr<QnArchiveStreamReader>& archiveReader) :
    QnAbstractDataConsumer(kLiveMediaQueueLen),
    m_decoder(new SeamlessVideoDecoder()),
    m_awaitJumpCounter(0),
    m_waitForBOF(false)
{
    connect(archiveReader.get(), &QnArchiveStreamReader::beforeJump, this, &PlayerDataConsumer::onBeforeJump, Qt::DirectConnection);
    connect(archiveReader.get(), &QnArchiveStreamReader::jumpOccured, this, &PlayerDataConsumer::onJumpOccured, Qt::DirectConnection);
    connect(archiveReader.get(), &QnArchiveStreamReader::jumpCanceled, this, &PlayerDataConsumer::onJumpCanceled, Qt::DirectConnection);
}

PlayerDataConsumer::~PlayerDataConsumer()
{
    stop();
}

void PlayerDataConsumer::pleaseStop()
{
    base_type::pleaseStop();
    m_queueWaitCond.wakeAll();
}

bool PlayerDataConsumer::canAcceptData() const
{
    return base_type::canAcceptData();
}

int PlayerDataConsumer::getBufferingMask() const
{
    // todo: implement me
    return 1;
}

void PlayerDataConsumer::clearOutputBuffers()
{

}

bool PlayerDataConsumer::processData(const QnAbstractDataPacketPtr& data)
{
    auto videoFrame = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
    if (videoFrame)
        return processVideoFrame(videoFrame);

    auto audioFrame = std::dynamic_pointer_cast<QnCompressedAudioData>(data);
    if (audioFrame)
        return processAudioFrame(audioFrame);

    return true; // just ignore unknown frame type
}

void PlayerDataConsumer::setNoDelay(const QSharedPointer<QVideoFrame>& frame) const
{
    FrameMetadata metadata = FrameMetadata::deserialize(frame);
    metadata.noDelay = true;
    metadata.serialize(frame);
}

bool PlayerDataConsumer::processVideoFrame(const QnCompressedVideoDataPtr& data)
{
    const bool isBofData = (data->flags & QnAbstractMediaData::MediaFlags_BOF); //< first packet after a jump
    bool displayImmediately = isBofData; //< display data immediately with no delay related to previous frame
    {
        QnMutexLocker lock(&m_dataProviderMutex);
        if (m_awaitJumpCounter > 0)
            displayImmediately = true; //< display any data immediately if more jumps are queued
        if (m_waitForBOF)
        {
            if (!isBofData)
                return true; //< Ignore data because we are waiting new data after a jump
            // It's a first packet after a final jump
            m_waitForBOF = false;
            clearOutputBuffers();
        }
    }

	QSharedPointer<QVideoFrame> decodedFrame;
	if (!m_decoder->decode(data, &decodedFrame))
	{
		qDebug() << Q_FUNC_INFO << "Can't decode video frame. skip it";
		return true; // false result means we want to repeat this frame latter
	}

    if (displayImmediately) 
    {
        setNoDelay(decodedFrame);
        {
            QnMutexLocker lock(&m_queueMutex);
            for (auto& frame : m_decodedVideo)
                setNoDelay(frame);
        }

        emit hurryUp(); //< hint to a player to avoid waiting for the currently displaying frame
    }
	enqueueVideoFrame(std::move(decodedFrame));


	return true;
}

void PlayerDataConsumer::enqueueVideoFrame(QSharedPointer<QVideoFrame> decodedFrame)
{
	QnMutexLocker lock(&m_queueMutex);
	while (m_decodedVideo.size() >= kMaxDecodedVideoQueueSize && !needToStop())
		m_queueWaitCond.wait(&m_queueMutex);
	if (needToStop())
		return;
	m_decodedVideo.push_back(std::move(decodedFrame));
	lock.unlock();
	emit gotVideoFrame();
}

QSharedPointer<QVideoFrame> PlayerDataConsumer::dequeueVideoFrame()
{
	QnMutexLocker lock(&m_queueMutex);
	if (m_decodedVideo.empty())
		return QSharedPointer<QVideoFrame>(); //< no data
	QSharedPointer<QVideoFrame> result = std::move(m_decodedVideo.front());
	m_decodedVideo.pop_front();
	lock.unlock();
	m_queueWaitCond.wakeAll();
	return result;
}

bool PlayerDataConsumer::processAudioFrame(const QnCompressedAudioDataPtr& /*data */)
{
	// todo: implement me
	return true;
}

void PlayerDataConsumer::onBeforeJump(qint64 /* timeUsec */)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    QnMutexLocker lock(&m_dataProviderMutex);
    ++m_awaitJumpCounter;
    m_buffering = getBufferingMask();
}

void PlayerDataConsumer::onJumpCanceled(qint64 /*timeUsec*/)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    // Previous jump command hasn't been executed due to new jump command received
    QnMutexLocker lock(&m_dataProviderMutex);
    --m_awaitJumpCounter;
    Q_ASSERT(m_awaitJumpCounter > 0);
}

void PlayerDataConsumer::onJumpOccured(qint64 /* timeUsec */)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    clearUnprocessedData(); //< Clear input (undecoded) data queue
    QnMutexLocker lock(&m_dataProviderMutex);
    if (--m_awaitJumpCounter == 0)
    {
        /*
        * The purpose of this variable is prevent doing delay between frames while they are displayed
        * We supposed to decode/display they at maximum speed unless lat jump command will be processed
        */
        m_waitForBOF = true; 
    }
}

}
}
