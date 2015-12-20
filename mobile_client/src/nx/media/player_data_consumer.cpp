#include "player_data_consumer.h"
#include "seamless_video_decoder.h"

namespace 
{
	static const int kLiveMediaQueueLen = 20;       //< default media queue line
	static const int kMaxDecodedVideoQueueSize = 2; //< max queue len for decoded video awaiting to be rendered
}

namespace nx
{
namespace media
{

PlayerDataConsumer::PlayerDataConsumer():
	QnAbstractDataConsumer(kLiveMediaQueueLen),
	m_decoder(new SeamlessVideoDecoder())
{
	
}

bool PlayerDataConsumer::canAcceptData() const
{
	// todo: implement me
	return true;
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

bool PlayerDataConsumer::processVideoFrame(const QnCompressedVideoDataPtr& data)
{
	QSharedPointer<QVideoFrame> decodedFrame;
	if (!m_decoder->decode(data, &decodedFrame))
	{
		qDebug() << Q_FUNC_INFO << "Can't decode video frame. skip it";
		return true; // false result means we want to repeat this frame latter
	}

	enqueueVideoFrame(std::move(decodedFrame));


	return true;
}

void PlayerDataConsumer::enqueueVideoFrame(QSharedPointer<QVideoFrame> decodedFrame)
{
	QnMutexLocker lock(&m_queueMutex);
	while (m_decodedVideo.size() >= kMaxDecodedVideoQueueSize)
		m_queueWaitCond.wait(&m_queueMutex);
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

bool PlayerDataConsumer::processAudioFrame(const QnCompressedAudioDataPtr& data)
{
	// todo: implement me
	return true;
}




}
}
