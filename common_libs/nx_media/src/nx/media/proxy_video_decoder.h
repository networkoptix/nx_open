#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

class ProxyVideoDecoderPrivate;

/**
 * Implements video decoder which delegates actual decoding to an external lib 'proxydecoder',
 * which is intended not to be the part of netoptix_vms tree, and has an interface not depending on
 * ffmpeg, thus, can be compiled with a different ffmpeg version.
 */
class ProxyVideoDecoder
:
    public AbstractVideoDecoder
{
public:
    ProxyVideoDecoder();

    virtual ~ProxyVideoDecoder();

    static bool isCompatible(const CodecID codec, const QSize& resolution);

    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) override;

    virtual void setAllocator(AbstractResourceAllocator* allocator) override;

private: 
    std::shared_ptr<ProxyVideoDecoderPrivate> d;
    friend class ProxyVideoDecoderPrivate;
};

} // namespace media
} // namespace nx
