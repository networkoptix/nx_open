#include "proxy_stream_data_receptor.h"

namespace nx::vms::server::analytics {

ProxyStreamDataReceptor::ProxyStreamDataReceptor(QWeakPointer<StreamDataReceptor> receptor):
    m_proxiedReceptor(std::move(receptor))
{
}

void ProxyStreamDataReceptor::setProxiedReceptor(QWeakPointer<StreamDataReceptor> receptor)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_proxiedReceptor = std::move(receptor);

    registerStreamsToProxiedReceptorUnsafe();
}

void ProxyStreamDataReceptor::putData(const QnAbstractDataPacketPtr& data)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (const auto proxiedReceptor = m_proxiedReceptor.toStrongRef())
        proxiedReceptor->putData(data);
}

StreamProviderRequirements ProxyStreamDataReceptor::providerRequirements(
    nx::vms::api::StreamIndex streamIndex) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (const auto proxiedReceptor = m_proxiedReceptor.toStrongRef())
        return proxiedReceptor->providerRequirements(streamIndex);

    return {};
}

void ProxyStreamDataReceptor::registerStream(nx::vms::api::StreamIndex streamIndex)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_registeredStreams.insert(streamIndex);

    registerStreamsToProxiedReceptorUnsafe();
}

void ProxyStreamDataReceptor::registerStreamsToProxiedReceptorUnsafe()
{
    if (const auto proxiedReceptor = m_proxiedReceptor.toStrongRef())
    {
        for (nx::vms::api::StreamIndex streamIndex: m_registeredStreams)
            proxiedReceptor->registerStream(streamIndex);
    }
}

} // namespace nx::vms::server::analytics
