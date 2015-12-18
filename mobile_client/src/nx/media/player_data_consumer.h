#pragma once

#include "nx/streaming/abstract_data_consumer.h"
#include "nx/streaming/video_data_packet.h"
#include "nx/streaming/audio_data_packet.h"

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
		public:
			PlayerDataConsumer();
		protected:
			virtual bool canAcceptData() const override;
			virtual bool processData(const QnAbstractDataPacketPtr& data) override;
		private:
			bool processVideoFrame(const QnCompressedVideoDataPtr& data);
			bool processAudioFrame(const QnCompressedAudioDataPtr& data);
		private:
			std::unique_ptr<SeamlessVideoDecoder> m_decoder;
		};
	}
}
