#ifdef ENABLE_VMAX

#include "vmax480_resource_proxy.h"

#include <plugins/storage/dts/vmax480/vmax480_resource.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

QnVmax480ResourceProxy::QnVmax480ResourceProxy(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connect(
        this->commonModule()->resourcePool(), &QnResourcePool::resourceAdded,
        this, &QnVmax480ResourceProxy::at_resourceAdded);

    connect(
        this->commonModule()->resourcePool(), &QnResourcePool::resourceRemoved,
        this, &QnVmax480ResourceProxy::at_resourceRemoved);
}

void QnVmax480ResourceProxy::at_resourceAdded(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    auto vmaxResource = resource.dynamicCast<QnPlVmax480Resource>();
    if (!vmaxResource)
        return;

    m_vmaxResourcesByGroupId[vmaxResource->getGroupId()].insert(vmaxResource);
}

void QnVmax480ResourceProxy::at_resourceRemoved(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    auto vmaxResource = resource.dynamicCast<QnPlVmax480Resource>();
    if (!vmaxResource)
        return;

    m_vmaxResourcesByGroupId[vmaxResource->getGroupId()].erase(vmaxResource);
}

QString QnVmax480ResourceProxy::url(const QString& groupId) const
{
    QnMutexLocker lock(&m_mutex);
    if (const auto resource = resourceUnsafe(groupId))
        return resource->getUrl();

    return QString();
}

QString QnVmax480ResourceProxy::hostAddress(const QString& groupId) const
{
    QnMutexLocker lock(&m_mutex);
    if (const auto resource = resourceUnsafe(groupId))
        return resource->getHostAddress();

    return QString();
}

QAuthenticator QnVmax480ResourceProxy::auth(const QString& groupId) const
{
    QnMutexLocker lock(&m_mutex);
    if (const auto resource = resourceUnsafe(groupId))
        return resource->getAuth();

    return QAuthenticator();
}

void QnVmax480ResourceProxy::setArchiveRange(
    const QString& groupId,
    qint64 startTimeUs,
    qint64 endTimeUs)
{
    QnMutexLocker lock(&m_mutex);
    if (auto resource = resourceUnsafe(groupId))
        resource->setArchiveRange(startTimeUs, endTimeUs);
}

QnPlVmax480ResourcePtr QnVmax480ResourceProxy::resourceUnsafe(const QString& groupId) const
{
    if (auto it = m_vmaxResourcesByGroupId.find(groupId); it != m_vmaxResourcesByGroupId.cend())
    {
        if (it->second.empty())
            return nullptr;

        return *(it->second.begin());
    }

    return nullptr;
}

#endif // ENABLE_VMAX
