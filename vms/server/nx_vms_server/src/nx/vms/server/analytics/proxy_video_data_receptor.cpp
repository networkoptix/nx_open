#include "proxy_video_data_receptor.h"

namespace nx::vms::server::analytics {

ProxyStreamDataReceptor::ProxyStreamDataReceptor(QWeakPointer<IStreamDataReceptor> receptor):
    m_proxiedReceptor(std::move(receptor))
{
}

void ProxyStreamDataReceptor::setProxiedReceptor(QWeakPointer<IStreamDataReceptor> receptor)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_proxiedReceptor = std::move(receptor);
}

void ProxyStreamDataReceptor::putData(const QnAbstractDataPacketPtr& data)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (const auto proxiedReceptor = m_proxiedReceptor.toStrongRef())
        proxiedReceptor->putData(data);
}

nx::vms::api::analytics::StreamTypes ProxyStreamDataReceptor::requiredStreamTypes() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (const auto proxiedReceptor = m_proxiedReceptor.toStrongRef())
        return proxiedReceptor->requiredStreamTypes();

    return {};
}

} // namespace nx::vms::server::analytics
