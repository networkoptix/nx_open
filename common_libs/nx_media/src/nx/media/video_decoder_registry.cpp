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
    const AVCodecID codec, const QSize& resolution, bool allowOverlay)
{
    QMutexLocker lock(&mutex);

    static std::map<AbstractVideoDecoder*, Metadata*> decodersInUse;
    for (auto& plugin: m_plugins)
    {
        if (plugin.useCount < plugin.maxUseCount
            && plugin.isCompatible(codec, resolution, allowOverlay))
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
    const AVCodecID codec, const QSize& resolution, bool allowOverlay)
{
    QMutexLocker lock(&mutex);
    for (const auto& plugin: m_plugins)
    {
        if (plugin.isCompatible(codec, resolution, allowOverlay)
            && plugin.useCount < plugin.maxUseCount)
        {
            return true;
        }
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
