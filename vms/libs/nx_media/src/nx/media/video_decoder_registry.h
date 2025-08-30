// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <typeindex>

#include <nx/media/media_fwd.h>
#include <nx/media/video_data_packet.h>

class QRhi;

namespace nx {
namespace media {

class AbstractVideoDecoder;
typedef std::unique_ptr<AbstractVideoDecoder, void(*)(AbstractVideoDecoder*)> VideoDecoderPtr;

template <typename T>
concept HasDecoderStaticMethods = requires(T a) //< 'a' is a dummy variable, not actually used.
{
    { T::isCompatible(
        /*codec*/ AV_CODEC_ID_H264,
        /*resolution*/ QSize{},
        /*allowHardwareAcceleration*/ true) } -> std::same_as<bool>;
    { T::maxResolution(/*codec*/ AV_CODEC_ID_H264) } -> std::same_as<QSize>;
};

/**
 * Singleton. Allows to register various implementations for video decoders. The exact list of
 * decoders can be registered in runtime.
 */
class NX_MEDIA_API VideoDecoderRegistry
{
public:
    static VideoDecoderRegistry* instance();

    /**
     * @return Optimal video decoder (in case of any) compatible with such frame. Return null
     * pointer if no compatible decoder is found.
     */
    VideoDecoderPtr createCompatibleDecoder(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowHardwareAcceleration,
        QRhi* rhi);

    /**
     * @return Whether a compatible video decoder is found.
     */
    bool hasCompatibleDecoder(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowHardwareAcceleration,
        const std::vector<AbstractVideoDecoder*>& currentDecoders);

    /**
     * @return Some sort of a maximum for all resolutions returned for the codec by
     * AbstractVideoDecoder::maxResolution(), or Invalid if it cannot be determined.
     */
    QSize maxResolution(const AVCodecID codec);

    /**
     * Register video decoder plugin.
     */
    template<class Decoder>
    void addPlugin(const QString& name, int maxUseCount = std::numeric_limits<int>::max())
    {
        m_plugins.push_back(MetadataImpl<Decoder>(name, maxUseCount));
    }

    /** For tests. */
    void reinitialize();

private:
    struct Metadata
    {
        std::function<AbstractVideoDecoder*(const QSize& resolution, QRhi* rhi)> createVideoDecoder;
        std::function<bool(
            const AVCodecID codec,
            const QSize& resolution,
            bool allowHardwareAcceleration)> isCompatible;
        std::function<QSize(const AVCodecID codec)> maxResolution;
        int useCount = 0;
        int maxUseCount = std::numeric_limits<int>::max();
        QString name;
        std::type_index typeIndex = std::type_index(typeid(nullptr));
    };

    template<HasDecoderStaticMethods Decoder>
    struct MetadataImpl: public Metadata
    {
        MetadataImpl(const QString& name, int maxUseCount)
        {
            createVideoDecoder =
                [](const QSize& resolution, QRhi* rhi)
                {
                    return new Decoder(resolution, rhi);
                };
            isCompatible = &Decoder::isCompatible;
            maxResolution = &Decoder::maxResolution;
            this->maxUseCount = maxUseCount;
            typeIndex = std::type_index(typeid(Decoder));
            this->name = name;
        }
    };

private:
    std::vector<Metadata> m_plugins;
};

} // namespace media
} // namespace nx
