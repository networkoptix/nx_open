#pragma once

#include <deque>
#include <atomic>

#include "nx/streaming/abstract_data_consumer.h"
#include "nx/streaming/video_data_packet.h"
#include "nx/streaming/audio_data_packet.h"

class QVideoFrame;
class QnArchiveStreamReader;

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
			typedef QnAbstractDataConsumer base_type;
		public:
            PlayerDataConsumer(const std::unique_ptr<QnArchiveStreamReader>& archiveReader);
			virtual ~PlayerDataConsumer();

			QSharedPointer<QVideoFrame> dequeueVideoFrame();
		signals:
			void gotVideoFrame();
        private slots:
            void onBeforeJump(qint64 timeUsec);
            void onJumpCanceled(qint64 timeUsec);
            void onJumpOccured(qint64 timeUsec);
		protected:
			virtual bool canAcceptData() const override;
			virtual bool processData(const QnAbstractDataPacketPtr& data) override;
		private:
			bool processVideoFrame(const QnCompressedVideoDataPtr& data);
			bool processAudioFrame(const QnCompressedAudioDataPtr& data);

			void enqueueVideoFrame(QSharedPointer<QVideoFrame> decodedFrame);
			virtual void pleaseStop() override;
		private:
			std::unique_ptr<SeamlessVideoDecoder> m_decoder;
			
			std::deque<QSharedPointer<QVideoFrame>> m_decodedVideo;
			QnWaitCondition m_queueWaitCond;
			QnMutex m_queueMutex;
            std::atomic<int> m_awaitJumpCounter;              //< how many jump requests are queued
		};
	}
}
