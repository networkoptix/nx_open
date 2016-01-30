#include "video_decoder_registry.h"
#include "abstract_video_decoder.h"

namespace nx {
namespace media {

VideoDecoderRegistry* VideoDecoderRegistry::instance()
{
	static VideoDecoderRegistry instance;
	return &instance;
}

std::unique_ptr<AbstractVideoDecoder> VideoDecoderRegistry::createCompatibleDecoder(const QnConstCompressedVideoDataPtr& frame)
{
	for (const auto& plugin : m_plugins)
	{
        if (plugin.isCompatible(frame)) 
        {
            auto result = VideoDecoderPtr(plugin.instance());
            if (plugin.allocator)
                result->setAllocator(plugin.allocator.get());
            return result;
        }
	}
	return VideoDecoderPtr(); //< no compatible decoder found
}

} // namespace media
} // namespace nx
