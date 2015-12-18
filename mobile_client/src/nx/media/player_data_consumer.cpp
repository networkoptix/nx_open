#include "player_data_consumer.h"
#include "seamless_video_decoder.h"

namespace 
{
	static const int kLiveMediaQueueLen = 20; // default media queue line
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
	if (!m_decoder->decode(data))
	{
		qDebug() << Q_FUNC_INFO << "Can't decode video frame. skip it";
		return true; // false result means we want to repeat this frame latter
	}

	return true;
}

bool PlayerDataConsumer::processAudioFrame(const QnCompressedAudioDataPtr& data)
{
	// todo: implement me
	return true;
}




}
}
