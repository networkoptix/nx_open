#include "video_decoder_registry.h"

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

VideoDecoderRegistry* VideoDecoderRegistry::instance()
{
    static VideoDecoderRegistry instance;
    return &instance;
}

std::unique_ptr<AbstractVideoDecoder> VideoDecoderRegistry::createCompatibleDecoder(
    const CodecID codec, const QSize& resolution)
{
    for (const auto& plugin: m_plugins)
    {
        if (plugin.isCompatible(codec, resolution))
        {
            auto result = VideoDecoderPtr(plugin.instance());
            if (plugin.allocator)
                result->setAllocator(plugin.allocator.get());
            return result;
        }
    }
    return VideoDecoderPtr(); //< no compatible decoder found
}

bool VideoDecoderRegistry::hasCompatibleDecoder(const CodecID codec, const QSize& resolution)
{
    for (const auto& plugin: m_plugins)
    {
        if (plugin.isCompatible(codec, resolution))
            return true;
    }
    return false;
}

} // namespace media
} // namespace nx
