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
 * Allows to register various implementations for video decoders. The exact list of decoders can be
 * registered in runtime.
 */
class VideoDecoderRegistry // singleton
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
     * @return Some sort of a maximum for all resolutions returned for the codec by
     * AbstractVideoDecoder::maxResolution(), or (0, 0) if it cannot be determined.
     */
    QSize maxResolution(const CodecID codec);

    bool isTranscodingEnabled() const;
    void setTranscodingEnabled(bool transcodingEnabled);

    /**
     * Register video decoder plugin.
     */
    template <class Decoder>
    void addPlugin(
        ResourceAllocatorPtr allocator = ResourceAllocatorPtr(),
        int maxUseCount = std::numeric_limits<int>::max())
    {
        m_plugins.push_back(MetadataImpl<Decoder>(allocator, maxUseCount));
    }

private:
    struct Metadata
    {
        Metadata()
        :
            useCount(0),
            maxUseCount(std::numeric_limits<int>::max())
        {
        }

        std::function<AbstractVideoDecoder* (
            const ResourceAllocatorPtr& allocator, const QSize& resolution)> createVideoDecoder;
        std::function<bool (const CodecID codec, const QSize& resolution)> isCompatible;
        std::function<QSize (const CodecID codec)> maxResolution;
        ResourceAllocatorPtr allocator;
        int useCount;
        int maxUseCount;
    };

    template <class Decoder>
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
