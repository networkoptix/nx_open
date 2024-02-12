// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

namespace nx::vms::client::core {

template<typename ResourceType>
class ResourceHolder: public CommonModuleAware
{
    using base_type = QObject;
    using ResourcePtr = QnSharedResourcePointer<ResourceType>;
public:
    ResourceHolder();
    ResourceHolder(const nx::Uuid& resourceId);

    bool setResourceId(nx::Uuid value);
    nx::Uuid resourceId() const;

    ResourcePtr resource();

private:
    nx::Uuid m_resourceId;
    ResourcePtr m_resource;
};

template<typename ResourceType>
ResourceHolder<ResourceType>::ResourceHolder()
{
}

template<typename ResourceType>
ResourceHolder<ResourceType>::ResourceHolder(const nx::Uuid& resourceId)
{
    setResourceId(resourceId);
}

template<typename ResourceType>
bool ResourceHolder<ResourceType>::setResourceId(nx::Uuid value)
{
    if (m_resourceId == value)
        return false;

    const auto pool = commonModule()->resourcePool();
    const auto target = value.isNull()
        ? ResourcePtr()
        : pool->template getResourceById<ResourceType>(value);

    NX_ASSERT(value.isNull() || target, "Can't find any resource with specified id and type!");

    if (!target)
        value = nx::Uuid();

    if (m_resourceId == value)
        return false;

    m_resourceId = value;
    m_resource = target;

    return true;
}

template<typename ResourceType>
nx::Uuid ResourceHolder<ResourceType>::resourceId() const
{
    return m_resourceId;
}

template<typename ResourceType>
typename ResourceHolder<ResourceType>::ResourcePtr ResourceHolder<ResourceType>::resource()
{
    return m_resource;
}

}; // namespace nx::vms::client::core
