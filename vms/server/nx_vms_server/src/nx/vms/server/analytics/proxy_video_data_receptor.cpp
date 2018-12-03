#include "proxy_video_data_receptor.h"

namespace nx::vms::server::analytics {

ProxyVideoDataReceptor::ProxyVideoDataReceptor(QWeakPointer<AbstractVideoDataReceptor> receptor):
    m_proxiedReceptor(std::move(receptor))
{
}

void ProxyVideoDataReceptor::setProxiedReceptor(QWeakPointer<AbstractVideoDataReceptor> receptor)
{
    QnMutexLocker lock(&m_mutex);
    m_proxiedReceptor = std::move(receptor);
}

bool ProxyVideoDataReceptor::needsCompressedFrames() const
{
    QnMutexLocker lock(&m_mutex);
    if (auto proxied = m_proxiedReceptor.toStrongRef())
        return proxied->needsCompressedFrames();

    return false;
}

AbstractVideoDataReceptor::NeededUncompressedPixelFormats
    ProxyVideoDataReceptor::neededUncompressedPixelFormats() const
{
    QnMutexLocker lock(&m_mutex);
    if (auto proxied = m_proxiedReceptor.toStrongRef())
        return proxied->neededUncompressedPixelFormats();

    return NeededUncompressedPixelFormats();
}

void ProxyVideoDataReceptor::putFrame(
    const QnConstCompressedVideoDataPtr& compressedFrame,
    const CLConstVideoDecoderOutputPtr& uncompressedFrame)
{
    QnMutexLocker lock(&m_mutex);
    if (auto proxied = m_proxiedReceptor.toStrongRef())
        proxied->putFrame(compressedFrame, uncompressedFrame);
}

} // namespace nx::vms::server::analytics
