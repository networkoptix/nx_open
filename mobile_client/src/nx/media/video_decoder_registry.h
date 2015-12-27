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

        /* This class allows to register various implementation for video decoders. Exact list of decoders can be registered in runtime mode */
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
			void addPlugin() { m_plugins << MetadataImpl<Decoder>(); }
		private:
			struct Metadata
			{
				std::function<AbstractVideoDecoder* ()> instance;
				std::function<bool(const QnConstCompressedVideoDataPtr& frame)> isCompatible;
			};

			template <class Decoder>
			struct MetadataImpl : public Metadata
			{
				MetadataImpl()
				{
					instance = []() { return new Decoder(); };
					isCompatible = &Decoder::isCompatible;
				}
			};

			QVector<Metadata> m_plugins;
		};
	}
}
