#include <common/common_module.h>
#include "cloud_user_info_pool.h"

CloudUserInfoPool::CloudUserInfoPool(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connectToResourcePool();
}

CloudUserInfoPool::~CloudUserInfoPool()
{
    disconnectFromResourcePool;
}

void CloudUserInfoPool::connectToResourcePool()
{
    connect(
        resourcePool(),
        &QnResourcePool::resourceAdded,
        this,
        &CloudUserInfoPool::onNewResource, Qt::QueuedConnection);
    connect(
        resourcePool(),
        &QnResourcePool::resourceRemoved,
        this,
        &CloudUserInfoPool::onRemoveResource,
        Qt::QueuedConnection);
}

void CloudUserInfoPool::onNewResource(const QnResourcePtr& resource)
{
    connect(
        resource,
        &QnResource::propertyChanged,
        this,
        [this](const QnResourcePtr& resource, const QString& key)
        {
            if (key != lit("CloudAuthInfo"))
                return;

            NX_LOG(lit("[CloudUserInfo] CloudAuthInfo changed for user %1. New value: %2")
                .arg(resource->name())
                .arg(resource->getProperty(key)));

            QnMutexLocker lock(&m_mutex);
        });
}

void CloudUserInfoPool::onRemoveResource(const QnResourcePtr& resource)
{
}

void CloudUserInfoPool::disconnectFromResourcePool()
{
}

bool CloudUserInfoPool::authenticate(const nx::http::header::Authorization& authHeader) const
{
}

boost::optional<nx::Buffer> CloudUserInfoPool::newestMostCommonNonce() const
{
}

QnResourcePool* CloudUserInfoPool::resourcePool()
{
    return qnCommonModule->resourcePool();
}
