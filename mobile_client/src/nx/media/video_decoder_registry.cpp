#include "video_decoder_registry.h"
#include "abstract_video_decoder.h"

namespace nx
{
namespace media
{

VideoDecoderRegistry* VideoDecoderRegistry::instance()
{
	VideoDecoderRegistry instance;
	return &instance;
}

std::unique_ptr<AbstractVideoDecoder> VideoDecoderRegistry::createCompatibleDecoder(const QnConstCompressedVideoDataPtr& frame)
{
	for (const auto& plugin : m_plugins)
	{
		if (plugin.isCompatible(frame))
			return VideoDecoderPtr(plugin.instance());
	}
	return VideoDecoderPtr(); // no compatible decoder found
}

}
}
