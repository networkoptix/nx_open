#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

namespace nx
{
	namespace media
	{
		class AbstractVideoDecoder;

		class VideoDecoderRegistry
		{
		public:
			static VideoDecoderRegistry* instance();
			/*
			* \returns optimal video decoder (in case of any) compatible with such frame. Returns null pointer if no compatible decoder found.
			*/
			typedef std::unique_ptr<AbstractVideoDecoder> VideoDecoderPtr;
			VideoDecoderPtr createCompatibleDecoder(const QnConstCompressedVideoDataPtr& frame);

			/** Register video decoder plugin */
			template <class Decoder>
			void addPlugin() { m_plugins.push_back(instantiate<Decoder>); }

		private:
			template <class Decoder>
			static AbstractVideoDecoder* instantiate(const QnConstCompressedVideoDataPtr& frame)
			{
				return Decoder::isCompatible(frame) ? new Decoder() : nullptr;
			};
		private:
			typedef std::function<AbstractVideoDecoder* (const QnConstCompressedVideoDataPtr& context)> InstanceFunc;
			QVector<InstanceFunc> m_plugins;
		};

	}
}
