// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "directly_shared_layouts_source.h"

#include <core/resource/layout_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>

namespace {

bool isSharedLayout(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<QnLayoutResource>();
    return layout && layout->isShared();
}

bool filter(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::layout))
        return false;

    return isSharedLayout(resource);
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::api;

DirectlySharedLayoutsSource::DirectlySharedLayoutsSource(
    const QnResourcePool* resourcePool,
    const QnGlobalPermissionsManager* globalPermissionsManager,
    const QnSharedResourcesManager* sharedResourcesManager,
    const QnResourceAccessSubject& accessSubject)
    :
    base_type(),
    m_resourcePool(resourcePool),
    m_globalPermissionsManager(globalPermissionsManager),
    m_sharedResourcesManager(sharedResourcesManager),
    m_accessSubject(accessSubject)
{
    connect(m_sharedResourcesManager, &QnSharedResourcesManager::sharedResourcesChanged, this,
        [this](const QnResourceAccessSubject& subject, const QSet<QnUuid>& oldValues,
            const QSet<QnUuid>& newValues)
        {
            if (subject != m_accessSubject)
                return;

            const auto rejectedResources = m_resourcePool->getResourcesByIds(oldValues - newValues);
            for (const auto& resource: rejectedResources)
            {
                if (filter(resource))
                    emit resourceRemoved(resource);
            }

            const auto newResources = m_resourcePool->getResourcesByIds(newValues - oldValues);
            for (const auto& resource: newResources)
            {
                if (filter(resource))
                    emit resourceAdded(resource);
            }
        });

    // QnSharedResourcesManager won't do any notifications when shared layout will be removed from
    // the resource pool, which seems not right. TODO: #vbreus Fix QnSharedResourcesManager, then
    // this workaround won't be needed anymore. Just in case, it's safe if resourceRemoved() will
    // be emitted twice.
    connect(resourcePool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::layout))
                return;

            if (isSharedLayout(resource))
                emit resourceRemoved(resource);
        });
}

QVector<QnResourcePtr> DirectlySharedLayoutsSource::getResources()
{
    if (m_globalPermissionsManager->hasGlobalPermission(m_accessSubject,
        GlobalPermission::adminPermissions))
    {
        NX_ASSERT(false, "Such query is unexpected");
        return {};
    }

    const auto resources = m_resourcePool->getResourcesByIds(
        m_sharedResourcesManager->sharedResources(m_accessSubject));

    QVector<QnResourcePtr> result;
    std::copy_if(std::cbegin(resources), std::cend(resources), std::back_inserter(result),
        [](const QnResourcePtr& resource) { return filter(resource); });

    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
