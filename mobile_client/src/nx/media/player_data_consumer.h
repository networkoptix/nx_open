#pragma once

#include <deque>

#include "nx/streaming/abstract_data_consumer.h"
#include "nx/streaming/video_data_packet.h"
#include "nx/streaming/audio_data_packet.h"

class QVideoFrame;
namespace nx
{
	namespace media
	{
		class SeamlessVideoDecoder;

		/*
		* Private class used in nx::media::Player
		*/
		class PlayerDataConsumer : public QnAbstractDataConsumer
		{
			Q_OBJECT
		public:
			PlayerDataConsumer();

			QSharedPointer<QVideoFrame> dequeueVideoFrame();
		signals:
			void gotVideoFrame();
		protected:
			virtual bool canAcceptData() const override;
			virtual bool processData(const QnAbstractDataPacketPtr& data) override;
		private:
			bool processVideoFrame(const QnCompressedVideoDataPtr& data);
			bool processAudioFrame(const QnCompressedAudioDataPtr& data);

			void enqueueVideoFrame(QSharedPointer<QVideoFrame> decodedFrame);
		private:
			std::unique_ptr<SeamlessVideoDecoder> m_decoder;
			
			std::deque<QSharedPointer<QVideoFrame>> m_decodedVideo;
			QnWaitCondition m_queueWaitCond;
			QnMutex m_queueMutex;
		};
	}
}
