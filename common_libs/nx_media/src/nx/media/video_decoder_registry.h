#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>
#include <QtGui/QOpenGLContext>

#include <nx/streaming/video_data_packet.h>
#include "abstract_resource_allocator.h"

namespace nx {
namespace media {

class AbstractVideoDecoder;
typedef std::unique_ptr<AbstractVideoDecoder, void(*)(AbstractVideoDecoder*)> VideoDecoderPtr;

/**
 * This class allows to register various implementations for video decoders. Exact list of decoders
 * can be registered in runtime mode.
 */
class VideoDecoderRegistry
{
public:
    static VideoDecoderRegistry* instance();

    /**
     * @return Optimal video decoder (in case of any) compatible with such frame. Return null
     * pointer if no compatible decoder is found.
     */
    VideoDecoderPtr createCompatibleDecoder(const CodecID codec, const QSize& resolution);

    /**
     * @return True if compatible video decoder found.
     */
    bool hasCompatibleDecoder(const CodecID codec, const QSize& resolution);

    /**
     * Register video decoder plugin.
     */
    template <class Decoder>
    void addPlugin(
        std::shared_ptr<AbstractResourceAllocator> allocator =
        std::shared_ptr<AbstractResourceAllocator>(),
        int maxUseCount = std::numeric_limits<int>::max())
    {
        m_plugins.push_back(MetadataImpl<Decoder>(allocator, maxUseCount));
    }

private:
    struct Metadata
    {
        Metadata():
            useCount(0),
            maxUseCount(std::numeric_limits<int>::max())
        {
        }

        std::function<AbstractVideoDecoder* ()> instance;
        std::function<bool(const CodecID codec, const QSize& resolution)> isCompatible;
        std::shared_ptr<AbstractResourceAllocator> allocator;
        int useCount;
        int maxUseCount;
    };

    template <class Decoder>
    struct MetadataImpl: public Metadata
    {
        MetadataImpl(std::shared_ptr<AbstractResourceAllocator> allocator, int maxUseCount)
        {
            instance =
                []()
                {
                    return new Decoder();
                };
            isCompatible = &Decoder::isCompatible;
            this->allocator = std::move(allocator);
            this->maxUseCount = maxUseCount;
        }
    };

    std::vector<Metadata> m_plugins;
};

} // namespace media
} // namespace nx
