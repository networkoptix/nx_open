#pragma once

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <client_core/connection_context_aware.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

template<typename ResourceType>
class ResourceHolder: public QnConnectionContextAware
{
    using base_type = QObject;
    using ResourcePtr = QnSharedResourcePointer<ResourceType>;
public:
    ResourceHolder();
    ResourceHolder(const QnUuid& resourceId);

    bool setResourceId(QnUuid value);
    QnUuid resourceId() const;

    ResourcePtr resource();

private:
    QnUuid m_resourceId;
    ResourcePtr m_resource;
};

template<typename ResourceType>
ResourceHolder<ResourceType>::ResourceHolder()
{
}

template<typename ResourceType>
ResourceHolder<ResourceType>::ResourceHolder(const QnUuid& resourceId)
{
    setResourceId(resourceId);
}

template<typename ResourceType>
bool ResourceHolder<ResourceType>::setResourceId(QnUuid value)
{
    if (m_resourceId == value)
        return false;

    const auto pool = commonModule()->resourcePool();
    const auto target = value.isNull()
        ? ResourcePtr()
        : pool->template getResourceById<ResourceType>(value);

    NX_ASSERT(value.isNull() || target, "Can't find any resource with specified id and type!");

    if (!target)
        value = QnUuid();

    if (m_resourceId == value)
        return false;

    m_resourceId = value;
    m_resource = target;

    return true;
}

template<typename ResourceType>
QnUuid ResourceHolder<ResourceType>::resourceId() const
{
    return m_resourceId;
}

template<typename ResourceType>
typename ResourceHolder<ResourceType>::ResourcePtr ResourceHolder<ResourceType>::resource()
{
    return m_resource;
}

}; // namespace nx::vms::client::core
