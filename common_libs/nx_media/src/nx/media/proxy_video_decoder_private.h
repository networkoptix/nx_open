#pragma once
#if defined(ENABLE_PROXY_DECODER)

#include <proxy_decoder.h>

#include "proxy_video_decoder.h"

namespace nx {
namespace media {

/**
 * Base class for different implementations of ProxyVideoDecoder functionality.
 */
class ProxyVideoDecoderPrivate // abstract
{
public:
    struct Params
    {
        ProxyVideoDecoder* owner;
        ResourceAllocatorPtr allocator;
        QSize resolution;
    };

    static ProxyVideoDecoderPrivate* createImplStub(const Params& params);
    static ProxyVideoDecoderPrivate* createImplDisplay(const Params& params);
    static ProxyVideoDecoderPrivate* createImplRgb(const Params& params);
    static ProxyVideoDecoderPrivate* createImplYuvPlanar(const Params& params);
    static ProxyVideoDecoderPrivate* createImplYuvNative(const Params& params);
    static ProxyVideoDecoderPrivate* createImplGl(const Params& params);

    ProxyVideoDecoderPrivate(const Params& params)
    :
        m_params(params),
        m_proxyDecoder(ProxyDecoder::create(params.resolution.width(), params.resolution.height()))
    {
    }

    virtual ~ProxyVideoDecoderPrivate() = default;

    /**
     * @param compressedVideoData Either is null, or has non-null data().
     * @return Value with the same semantics as AbstractVideoDecoder::decode().
     */
    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) = 0;

protected:
    const std::shared_ptr<ProxyVideoDecoderPrivate>& sharedPtrToThis() const
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

private:
    Params m_params;
    std::unique_ptr<ProxyDecoder> m_proxyDecoder;
};

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
