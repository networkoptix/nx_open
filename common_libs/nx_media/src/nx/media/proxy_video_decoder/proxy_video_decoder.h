#pragma once
#if defined(ENABLE_PROXY_DECODER)

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include <nx/media/abstract_video_decoder.h>

namespace nx {
namespace media {

class ProxyVideoDecoderImpl;

/**
 * Implements video decoder which delegates actual decoding to an external lib 'proxydecoder',
 * which is intended not to be the part of nx_vms tree, and has an interface not depending on
 * ffmpeg or any other libs, thus, can be compiled with a different ffmpeg version.
 */
class ProxyVideoDecoder:
    public AbstractVideoDecoder
{
public:
    ProxyVideoDecoder(const ResourceAllocatorPtr& allocator, const QSize& resolution);

    virtual ~ProxyVideoDecoder();

    static bool isCompatible(
        const AVCodecID codec, const QSize& resolution, bool allowOverlay);

    static QSize maxResolution(const AVCodecID codec);

    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) override;

    virtual Capabilities capabilities() const override;
private:
    std::shared_ptr<ProxyVideoDecoderImpl> d;
    friend class ProxyVideoDecoderImpl;
};

} // namespace media
} // namespace nx

#endif // defined(ENABLE_PROXY_DECODER)
