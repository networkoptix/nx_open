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
 * Singleton. Allows to register various implementations for video decoders. The exact list of
 * decoders can be registered in runtime.
 */
class VideoDecoderRegistry
{
public:
    static VideoDecoderRegistry* instance();

    /**
     * @return Optimal video decoder (in case of any) compatible with such frame. Return null
     * pointer if no compatible decoder is found.
     */
    VideoDecoderPtr createCompatibleDecoder(
        const AVCodecID codec, const QSize& resolution, bool allowOverlay);

    /**
     * @return Whether a compatible video decoder is found.
     */
    bool hasCompatibleDecoder(
        const AVCodecID codec, const QSize& resolution, bool allowOverlay);

    /**
     * @return Some sort of a maximum for all resolutions returned for the codec by
     * AbstractVideoDecoder::maxResolution(), or Invalid if it cannot be determined.
     */
    QSize maxResolution(const AVCodecID codec);

    /**
     * @return Whether transcoding is not explicitly disabled by the client due to some reason,
     * e.g. by resetting this flag to false, Mobile Client in Lite Mode may prefer requesting low
     * stream instead of requesting transcoding.
     */
    bool isTranscodingEnabled() const;

    void setTranscodingEnabled(bool transcodingEnabled);

    /**
     * Register video decoder plugin.
     */
    template<class Decoder>
    void addPlugin(
        ResourceAllocatorPtr allocator = ResourceAllocatorPtr(),
        int maxUseCount = std::numeric_limits<int>::max())
    {
        m_plugins.push_back(MetadataImpl<Decoder>(allocator, maxUseCount));
    }

    /** For tests. */
    void reinitialize();

private:
    struct Metadata
    {
        Metadata():
            useCount(0),
            maxUseCount(std::numeric_limits<int>::max())
        {
        }

        std::function<AbstractVideoDecoder*(
            const ResourceAllocatorPtr& allocator, const QSize& resolution)> createVideoDecoder;
        std::function<bool(
            const AVCodecID codec, const QSize& resolution, bool allowOverlay)> isCompatible;
        std::function<QSize(const AVCodecID codec)> maxResolution;
        ResourceAllocatorPtr allocator;
        int useCount;
        int maxUseCount;
    };

    template<class Decoder>
    struct MetadataImpl: public Metadata
    {
        MetadataImpl(ResourceAllocatorPtr allocator, int maxUseCount)
        {
            createVideoDecoder =
                [](const ResourceAllocatorPtr& allocator, const QSize& resolution)
                {
                    return new Decoder(allocator, resolution);
                };
            isCompatible = &Decoder::isCompatible;
            maxResolution = &Decoder::maxResolution;
            this->allocator = std::move(allocator);
            this->maxUseCount = maxUseCount;
        }
    };

    std::vector<Metadata> m_plugins;

    bool m_isTranscodingEnabled;
};

} // namespace media
} // namespace nx
