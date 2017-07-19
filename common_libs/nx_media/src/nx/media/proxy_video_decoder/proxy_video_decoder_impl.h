#pragma once
#if defined(ENABLE_PROXY_DECODER)

#include <proxy_decoder.h>

#include "proxy_video_decoder.h"

namespace nx {
namespace media {

/**
 * Abstract base class for different implementations of ProxyVideoDecoder functionality.
 */
class ProxyVideoDecoderImpl
{
public:
    struct Params
    {
        ProxyVideoDecoder* owner;
        ResourceAllocatorPtr allocator;
        QSize resolution;
    };

    static ProxyVideoDecoderImpl* createImplStub(const Params& params);
    static ProxyVideoDecoderImpl* createImplDisplay(const Params& params);
    static ProxyVideoDecoderImpl* createImplRgb(const Params& params);
    static ProxyVideoDecoderImpl* createImplYuvPlanar(const Params& params);
    static ProxyVideoDecoderImpl* createImplYuvNative(const Params& params);
    static ProxyVideoDecoderImpl* createImplGl(const Params& params);

    ProxyVideoDecoderImpl(const Params& params):
        m_params(params),
        m_proxyDecoder(ProxyDecoder::create(params.resolution.width(), params.resolution.height()))
    {
    }

    virtual ~ProxyVideoDecoderImpl() = default;

    /**
     * @param compressedVideoData Either is null, or has non-null data().
     * @return Value with the same semantics as AbstractVideoDecoder::decode().
     */
    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) = 0;

protected:
    const std::shared_ptr<ProxyVideoDecoderImpl>& sharedPtrToThis() const
    {
        return m_params.owner->d;
    }

    QSize frameSize() const
    {
        return m_params.resolution;
    }

    AbstractResourceAllocator& allocator()
    {
        return *m_params.allocator.get();
    }

    ProxyDecoder& proxyDecoder() const
    {
        return *m_proxyDecoder.get();
    }

    QRect getVideoGeometry() const
    {
        return m_params.owner->getVideoGeometry();
    }

    void setQVideoFrame(
        QVideoFramePtr* outDecodedFrame,
        QAbstractVideoBuffer* videoBuffer,
        QVideoFrame::PixelFormat format,
        int64_t ptsUs)
    {
        outDecodedFrame->reset(new QVideoFrame(videoBuffer, frameSize(), format));

        // TODO: QVideoFrame doc states that PTS should be in microseconds, but everywhere in
        // nx_media it is treated as if it were milliseconds. Fix with reviewing all usages.
        (*outDecodedFrame)->setStartTime(ptsUs / 1000);
    }

private:
    Params m_params;
    std::unique_ptr<ProxyDecoder> m_proxyDecoder;
};

} // namespace media
} // namespace nx

#endif // defined(ENABLE_PROXY_DECODER)
