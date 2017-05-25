#include <common/common_module.h>
#include "cloud_user_info_pool.h"

CloudUserInfoPool::CloudUserInfoPool()
{
    connectToResourcePool();
}

CloudUserInfoPool::~CloudUserInfoPool()
{
    disconnectFromResourcePool;
}

void CloudUserInfoPool::connectToResourcePool()
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this, &QnRecordingManager::onNewResource, Qt::QueuedConnection);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, &QnRecordingManager::onRemoveResource, Qt::QueuedConnection);
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
