// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_api_backend.h"

#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_context.h>

#include "helpers.h"
#include "resources_structures.h"

namespace nx::vms::client::desktop::jsapi::detail {

/**
 * Show if we currently support specific resource type in API and user has enough access
 * rights.
 */
bool isResourceAvailable(const QnResourcePtr& resource)
{
    return resource
        && detail::resourceType(resource) != detail::ResourceType::undefined
        && ResourceAccessManager::hasPermissions(resource, Qn::ViewContentPermission);
}

//-------------------------------------------------------------------------------------------------

ResourcesApiBackend::ResourcesApiBackend(QObject* parent):
    base_type(parent)
{
    const auto addResources =
        [this](const QnResourceList& resources)
        {
            const auto addedResources = resources.filtered(
                [](const QnResourcePtr& resource)
                {
                    return isResourceAvailable(resource);
                });

            for (const auto& resource: addedResources)
                emit added(detail::Resource::from(resource));
        };

    const auto removeResources =
        [this](const QnResourceList& resources)
        {
            // Do not check availability since after removal we don't have any permission
            // for deleted resource.
            for (const auto& resource: resources)
                emit removed(ResourceUniqueId::from(resource));
        };

    const auto pool = appContext()->unifiedResourcePool();
    connect(pool, &UnifiedResourcePool::resourcesAdded, this, addResources);
    connect(pool, &UnifiedResourcePool::resourcesRemoved, this, removeResources);
}

ResourcesApiBackend::~ResourcesApiBackend()
{
}

detail::Resource::List ResourcesApiBackend::resources() const
{
    const auto resources = appContext()->unifiedResourcePool()->resources(
        [](const QnResourcePtr& resource)
        {
            return isResourceAvailable(resource);
        });
    return detail::Resource::from(resources);
}

detail::ResourceResult ResourcesApiBackend::resource(const ResourceUniqueId& resourceId) const
{
    const auto pool = appContext()->unifiedResourcePool();
    const auto resource = pool->resource(
        resourceId.id,
        resourceId.systemId.isNull()
            ? appContext()->currentSystemContext()->localSystemId()
            : resourceId.systemId);

    if (isResourceAvailable(resource))
        return detail::ResourceResult{Error::success(), detail::Resource::from(resource)};

    return detail::ResourceResult{
        Error::failed(tr("Resource is not available for the usage with JS API")), {}};
}

} // namespace nx::vms::client::desktop::jsapi::detail
