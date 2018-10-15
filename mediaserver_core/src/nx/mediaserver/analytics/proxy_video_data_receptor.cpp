#include "proxy_video_data_receptor.h"

namespace nx::mediaserver::analytics {

ProxyVideoDataReceptor::ProxyVideoDataReceptor(AbstractVideoDataReceptorPtr receptor):
    m_proxiedReceptor(std::move(receptor))
{
}

void ProxyVideoDataReceptor::setProxiedReceptor(AbstractVideoDataReceptorPtr receptor)
{
    QnMutexLocker lock(&m_mutex);
    m_proxiedReceptor = std::move(receptor);
}

bool ProxyVideoDataReceptor::needsCompressedFrames() const
{
    QnMutexLocker lock(&m_mutex);
    if (m_proxiedReceptor)
        return m_proxiedReceptor->needsCompressedFrames();

    return false;
}

AbstractVideoDataReceptor::NeededUncompressedPixelFormats
    ProxyVideoDataReceptor::neededUncompressedPixelFormats() const
{
    QnMutexLocker lock(&m_mutex);
    if (m_proxiedReceptor)
        return m_proxiedReceptor->neededUncompressedPixelFormats();

    return NeededUncompressedPixelFormats();
}

void ProxyVideoDataReceptor::putFrame(
    const QnConstCompressedVideoDataPtr& compressedFrame,
    const CLConstVideoDecoderOutputPtr& uncompressedFrame)
{
    QnMutexLocker lock(&m_mutex);
    if (m_proxiedReceptor)
        m_proxiedReceptor->putFrame(compressedFrame, uncompressedFrame);
}

} // namespace nx::mediaserver::analytics
