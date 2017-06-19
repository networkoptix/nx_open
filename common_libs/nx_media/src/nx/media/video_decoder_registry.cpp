#include "video_decoder_registry.h"

#include <unordered_map>

#include <QtCore/QMutexLocker>

#include <nx/utils/log/log.h>

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
    const AVCodecID codec, const QSize& resolution)
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

bool VideoDecoderRegistry::hasCompatibleDecoder(
    const AVCodecID codec, const QSize& resolution,
    const std::vector<AbstractVideoDecoder*>& currentDecoders)
{
    auto codecString =
        [codec, &resolution]()
        {
            return lit("%1 [%2x%3]").arg(codec).arg(resolution.width()).arg(resolution.height());
        };

    NX_LOGX(lm("Checking for decoder compatible with codec %1.").arg(codecString()), cl_logDEBUG2);

    // Some decoders can be used a limited number of times, e.g., once. Besides, this check could
    // happen when a player is playing video and thus acquired one or more decoders. The check may
    // fail if we don't take into account that the player can reuse its decoders.

    // TODO: This still not 100% correct. E.g., if a player already acquired a decoder which can
    // be used only once, then this check will report true regardless of what the player is doing:
    // switching the video quality or creating an additional decoder for multi-sensor camera.
    // So far this isn't critical, because for such cameras the player uses ffmpeg decoder which
    // doesn't have a count limit.

    std::unordered_map<std::type_index, int> decoderCountByTypeIndex;
    for (const auto& decoder: currentDecoders)
        ++decoderCountByTypeIndex[std::type_index(typeid(*decoder))];

    QMutexLocker lock(&mutex);
    for (const auto& plugin: m_plugins)
    {
        const int availableUsageCount =
            plugin.useCount - decoderCountByTypeIndex[plugin.typeIndex];

        if (availableUsageCount >= plugin.maxUseCount)
        {
            NX_LOGX(lm("Count exceeded for plugin %1").arg(plugin.name), cl_logDEBUG2);
            continue;
        }

        if (!plugin.isCompatible(codec, resolution))
        {
            NX_LOGX(
                lm("Plugin %1 is not compatible with codec %2").arg(plugin.name).arg(codecString()),
                cl_logDEBUG2);
            continue;
        }

        NX_LOGX(lm("Selected plugin: %1").arg(plugin.name), cl_logDEBUG2);
        return true;
    }
    return false;
}

QSize VideoDecoderRegistry::maxResolution(const AVCodecID codec)
{
    // Currently here we compare resolutions by height (number of lines).
    QMutexLocker lock(&mutex);
    QSize result;
    for (const auto& plugin: m_plugins)
    {
        const QSize resolution = plugin.maxResolution(codec);
        if (!resolution.isEmpty() && resolution.height() > result.height())
            result = resolution;
    }
    return result;
}

bool VideoDecoderRegistry::isTranscodingEnabled() const
{
    return m_isTranscodingEnabled;
}

void VideoDecoderRegistry::setTranscodingEnabled(bool transcodingEnabled)
{
    m_isTranscodingEnabled = transcodingEnabled;
}

void VideoDecoderRegistry::reinitialize()
{
    m_plugins.clear();
    m_isTranscodingEnabled = false;
}

} // namespace media
} // namespace nx
