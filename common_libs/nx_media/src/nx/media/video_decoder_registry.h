#pragma once

#include <functional>
#include <typeindex>

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>
#include <QtGui/QOpenGLContext>

#include <nx/media/media_fwd.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/utils/log/log.h>

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
    VideoDecoderPtr createCompatibleDecoder(const AVCodecID codec, const QSize& resolution,
        bool allowOverlay, ResourceAllocatorPtr resourceAllocator);

    /**
     * @return Whether a compatible video decoder is found.
     */
    bool hasCompatibleDecoder(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowOverlay,
        const std::vector<AbstractVideoDecoder*>& currentDecoders);

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
    void addPlugin(int maxUseCount = std::numeric_limits<int>::max())
    {
        m_plugins.push_back(MetadataImpl<Decoder>(maxUseCount));
    }

    /** For tests. */
    void reinitialize();

    ResourceAllocatorPtr defaultResourceAllocator() const;
    void setDefaultResourceAllocator(ResourceAllocatorPtr value);

    static QSize platformMaxFfmpegResolution();

private:
    struct Metadata
    {
        std::function<AbstractVideoDecoder*(
            const ResourceAllocatorPtr& allocator, const QSize& resolution)> createVideoDecoder;
        std::function<bool(
            const AVCodecID codec, const QSize& resolution, bool allowOverlay)> isCompatible;
        std::function<QSize(const AVCodecID codec)> maxResolution;
        int useCount = 0;
        int maxUseCount = std::numeric_limits<int>::max();
        QString name;
        std::type_index typeIndex = std::type_index(typeid(nullptr));
    };

    template<class Decoder>
    struct MetadataImpl: public Metadata
    {
        MetadataImpl(int maxUseCount)
        {
            createVideoDecoder =
                [](const ResourceAllocatorPtr& allocator, const QSize& resolution)
                {
                    return new Decoder(allocator, resolution);
                };
            isCompatible = &Decoder::isCompatible;
            maxResolution = &Decoder::maxResolution;
            this->maxUseCount = maxUseCount;
            typeIndex = std::type_index(typeid(Decoder));
            name = ::toString(typeid(Decoder));
        }
    };

    std::vector<Metadata> m_plugins;

    bool m_isTranscodingEnabled;

    ResourceAllocatorPtr m_defaultResourceAllocator;
};

} // namespace media
} // namespace nx
