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
            /* Hint to render to display current data with no delay due to seek operation in progress */
            void hurryUp();

            /* New video frame is decoded and ready to render */
			void gotVideoFrame();
        private slots:
            void onBeforeJump(qint64 timeUsec);
            void onJumpCanceled(qint64 timeUsec);
            void onJumpOccured(qint64 timeUsec);
		protected:
			virtual bool canAcceptData() const override;
			virtual bool processData(const QnAbstractDataPacketPtr& data) override;
            virtual void pleaseStop() override;
        private:
			bool processVideoFrame(const QnCompressedVideoDataPtr& data);
			bool processAudioFrame(const QnCompressedAudioDataPtr& data);

			void enqueueVideoFrame(QSharedPointer<QVideoFrame> decodedFrame);
            int getBufferingMask() const;
            void clearOutputBuffers();
            void setNoDelay(const QSharedPointer<QVideoFrame>& frame) const;
		private:
			std::unique_ptr<SeamlessVideoDecoder> m_decoder;
			
			std::deque<QSharedPointer<QVideoFrame>> m_decodedVideo;
			QnWaitCondition m_queueWaitCond;
			QnMutex m_queueMutex;        //< sync with player thread
            QnMutex m_dataProviderMutex; //< sync with dataProvider thread

            int m_awaitJumpCounter; //< how many jump requests are queued
            bool m_waitForBOF;
            int m_buffering;
		};
	}
}
