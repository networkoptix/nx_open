#include "audio_decoder_registry.h"
#include "abstract_audio_decoder.h"

namespace nx {
namespace media {

AudioDecoderRegistry* AudioDecoderRegistry::instance()
{
    static AudioDecoderRegistry instance;
    return &instance;
}

std::unique_ptr<AbstractAudioDecoder> AudioDecoderRegistry::createCompatibleDecoder(const AVCodecID codec)
{
    for (const auto& plugin : m_plugins)
    {
        if (plugin.isCompatible(codec)) 
            return AudioDecoderPtr(plugin.instance());
    }
    return AudioDecoderPtr(); //< no compatible decoder found
}

bool AudioDecoderRegistry::hasCompatibleDecoder(const AVCodecID codec)
{
    for (const auto& plugin : m_plugins)
    {
        if (plugin.isCompatible(codec))
            return true;
    }
    return false;
}

} // namespace media
} // namespace nx
