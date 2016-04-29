#include "video_decoder_registry.h"

#include <QtCore/QMutexLocker>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

namespace {

static QMutex mutex;

} // namespace

VideoDecoderRegistry* VideoDecoderRegistry::instance()
{
    static VideoDecoderRegistry instance;
    return &instance;
}

VideoDecoderPtr VideoDecoderRegistry::createCompatibleDecoder(
    const CodecID codec, const QSize& resolution)
{
    QMutexLocker lock(&mutex);

    static std::map<AbstractVideoDecoder*, Metadata*> decodersInUse;
    for (auto& plugin: m_plugins)
    {
        if (plugin.useCount < plugin.maxUseCount && plugin.isCompatible(codec, resolution))
        {
            auto videoDecoder = VideoDecoderPtr(
                plugin.createVideoDecoder(plugin.allocator, resolution),
                [](AbstractVideoDecoder* decoder)
                {
                    QMutexLocker lock(&mutex);
                    auto itr = decodersInUse.find(decoder);
                    if (itr != decodersInUse.end())
                    {
                        --itr->second->useCount;
                        decodersInUse.erase(itr);
                    }
                    delete decoder;
                });
            ++plugin.useCount;
            decodersInUse[videoDecoder.get()] = &plugin;
            return videoDecoder;
        }
    }
    return VideoDecoderPtr(nullptr, nullptr); //< no compatible decoder found
}

bool VideoDecoderRegistry::hasCompatibleDecoder(const CodecID codec, const QSize& resolution)
{
    QMutexLocker lock(&mutex);
    for (const auto& plugin: m_plugins)
    {
        if (plugin.isCompatible(codec, resolution) && plugin.useCount < plugin.maxUseCount)
            return true;
    }
    return false;
}

} // namespace media
} // namespace nx
