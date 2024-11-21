// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_decoder_registry.h"
#include "abstract_video_decoder.h"

#include <unordered_map>

#include <QtCore/QMutexLocker>

#include <nx/utils/log/log.h>

#include <nx/build_info.h>

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
    const AVCodecID codec,
    const QSize& resolution,
    bool allowOverlay,
    bool allowHardwareAcceleration,
    RenderContextSynchronizerPtr renderContextSynchronizer)
{
    QMutexLocker lock(&mutex);
    auto codecString =
        [codec, &resolution]()
        {
            return QString("%1 [%2x%3]")
                .arg(codec)
                .arg(resolution.width())
                .arg(resolution.height());
        };


    static std::map<AbstractVideoDecoder*, Metadata*> decodersInUse;
    for (auto& plugin: m_plugins)
    {
        NX_DEBUG(this, "Checking video decoder: %1 with codec: %2", plugin.name, codecString());
        if (plugin.useCount < plugin.maxUseCount
            && plugin.isCompatible(codec, resolution, allowOverlay, allowHardwareAcceleration))
        {
            auto videoDecoder = VideoDecoderPtr(
                plugin.createVideoDecoder(renderContextSynchronizer, resolution),
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
            NX_DEBUG(this, "Selected video decoder: %1", plugin.name);
            return videoDecoder;
        }
    }
    return VideoDecoderPtr(nullptr, nullptr); //< no compatible decoder found
}

bool VideoDecoderRegistry::hasCompatibleDecoder(
    const AVCodecID codec,
    const QSize& resolution,
    bool allowOverlay,
    bool allowHardwareAcceleration,
    const std::vector<AbstractVideoDecoder*>& currentDecoders)
{
    auto codecString =
        [codec, &resolution]()
        {
            return QString("%1 [%2x%3]")
                .arg(codec)
                .arg(resolution.width())
                .arg(resolution.height());
        };

    NX_VERBOSE(this, nx::format("Checking for decoder compatible with codec %1.").arg(codecString()));

    // Some decoders can be used a limited number of times, e.g., once. Besides, this check could
    // happen when a player is playing video and thus acquired one or more decoders. The check may
    // fail if we don't take into account that the player can reuse its decoders.

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
            NX_VERBOSE(this, nx::format("Count exceeded for plugin %1").arg(plugin.name));
            continue;
        }

        if (!plugin.isCompatible(codec, resolution, allowOverlay, allowHardwareAcceleration))
        {
            NX_VERBOSE(this, nx::format("Plugin %1 is not compatible with codec %2").arg(plugin.name).arg(codecString()));
            continue;
        }

        NX_VERBOSE(this, nx::format("Selected plugin: %1").arg(plugin.name));
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

RenderContextSynchronizerPtr VideoDecoderRegistry::defaultRenderContextSynchronizer() const
{
    return m_defaultRenderContextSynchronizer;
}

void VideoDecoderRegistry::setDefaultRenderContextSynchronizer(RenderContextSynchronizerPtr value)
{
    m_defaultRenderContextSynchronizer = value;
}

void VideoDecoderRegistry::reinitialize()
{
    m_plugins.clear();
    m_defaultRenderContextSynchronizer = RenderContextSynchronizerPtr();
}

} // namespace media
} // namespace nx
