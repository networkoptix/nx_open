#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>
#include <QOpenGLContext>

#include <nx/streaming/video_data_packet.h>
#include "abstract_resource_allocator.h"

namespace nx {
namespace media {
class AbstractVideoDecoder;
typedef std::unique_ptr<AbstractVideoDecoder> VideoDecoderPtr;

/* 
* This class allows to register various implementation for video decoders. Exact list of decoders can be registered in runtime mode 
*/
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
    void addPlugin(std::shared_ptr<AbstractResourceAllocator> allocator = std::shared_ptr<AbstractResourceAllocator>())
    {
        m_plugins.push_back(MetadataImpl<Decoder>(allocator));
    }

private:
	struct Metadata
	{
        Metadata::Metadata() {}

		std::function<AbstractVideoDecoder* ()> instance;
		std::function<bool(const QnConstCompressedVideoDataPtr& frame)> isCompatible;
        std::shared_ptr<AbstractResourceAllocator> allocator;
	};

	template <class Decoder>
	struct MetadataImpl : public Metadata
	{
        MetadataImpl(std::shared_ptr<AbstractResourceAllocator> allocator)
		{
			instance = []() { return new Decoder(); };
			isCompatible = &Decoder::isCompatible;
            this->allocator = std::move(allocator);
		}
	};

    std::vector<Metadata> m_plugins;
};

}
}
