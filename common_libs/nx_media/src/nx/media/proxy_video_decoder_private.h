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
    static ProxyVideoDecoderPrivate* createImplDisplay(
        ProxyVideoDecoder* owner, const ResourceAllocatorPtr& allocator, const QSize& resolution);
    static ProxyVideoDecoderPrivate* createImplRgb(
        ProxyVideoDecoder* owner, const ResourceAllocatorPtr& allocator, const QSize& resolution);
    static ProxyVideoDecoderPrivate* createImplYuvPlanar(
        ProxyVideoDecoder* owner, const ResourceAllocatorPtr& allocator, const QSize& resolution);
    static ProxyVideoDecoderPrivate* createImplYuvNative(
        ProxyVideoDecoder* owner, const ResourceAllocatorPtr& allocator, const QSize& resolution);
    static ProxyVideoDecoderPrivate* createImplGl(
        ProxyVideoDecoder* owner, const ResourceAllocatorPtr& allocator, const QSize& resolution);

    ProxyVideoDecoderPrivate(
        ProxyVideoDecoder* owner, const ResourceAllocatorPtr& allocator, const QSize& resolution)
    :
        m_owner(owner),
        m_frameSize(resolution),
        m_allocator(allocator),
        m_proxyDecoder(ProxyDecoder::create(resolution.width(), resolution.height()))
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
        return m_owner->d;
    }

    QSize frameSize() const
    {
        return m_frameSize;
    }

    AbstractResourceAllocator& allocator()
    {
        return *m_allocator.get();
    }

    ProxyDecoder& proxyDecoder() const
    {
        return *m_proxyDecoder.get();
    }

private:
    ProxyVideoDecoder* m_owner;
    const QSize m_frameSize;
    ResourceAllocatorPtr m_allocator;
    std::unique_ptr<ProxyDecoder> m_proxyDecoder;
};

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
