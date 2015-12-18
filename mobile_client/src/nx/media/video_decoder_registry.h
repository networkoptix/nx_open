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
		typedef std::unique_ptr<AbstractVideoDecoder> VideoDecoderPtr;

		class VideoDecoderRegistry
		{
		public:
			static VideoDecoderRegistry* instance();
			/*
			* \returns optimal video decoder (in case of any) compatible with such frame. Returns null pointer if no compatible decoder found.
			*/
			VideoDecoderPtr createCompatibleDecoder(const QnConstCompressedVideoDataPtr& frame);

			/** Register video decoder plugin */
			template <class Decoder>
			void addPlugin() { m_plugins << Metadata<Decoder>(); }
		private:
			struct MetadataBase
			{
				std::function<AbstractVideoDecoder* ()> instance;
				std::function<bool(const QnConstCompressedVideoDataPtr& frame)> isCompatible;
			};

			template <class Decoder>
			struct Metadata : public MetadataBase
			{
				Metadata()
				{
					instance = []() { return new Decoder(); };
					isCompatible = &Decoder::isCompatible;
				}
			};

			QVector<MetadataBase> m_plugins;
		};
	}
}
