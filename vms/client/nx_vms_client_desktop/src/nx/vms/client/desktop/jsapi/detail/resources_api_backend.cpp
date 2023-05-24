// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_api_backend.h"

#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_access_controller.h>
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
                emit removed(resource->getId());
        };

    const auto pool = appContext()->currentSystemContext()->resourcePool();
    connect(pool, &QnResourcePool::resourcesAdded, this, addResources);
    connect(pool, &QnResourcePool::resourcesRemoved, this, removeResources);
}

ResourcesApiBackend::~ResourcesApiBackend()
{
}

detail::Resource::List ResourcesApiBackend::resources() const
{
    // Only current system's resources are presented in the list of available resources, as only
    // such items can be added on a layout.
    const auto resources = appContext()->currentSystemContext()->resourcePool()->getResources(
        [](const QnResourcePtr& resource)
        {
            return isResourceAvailable(resource);
        });
    return detail::Resource::from(resources);
}

detail::ResourceResult ResourcesApiBackend::resource(const QUuid& resourceId) const
{
    // Use UnifiedResourcePool to make it possible to interact with cross-system items on a layout
    // although only items of the current system are presented in the list of available resources.
    const auto resource = appContext()->unifiedResourcePool()->resource(resourceId);
    if (isResourceAvailable(resource))
        return detail::ResourceResult{Error::success(), detail::Resource::from(resource)};

    return detail::ResourceResult{
        Error::failed(tr("Resource is not available for the usage with JS API")), {}};
}

} // namespace nx::vms::client::desktop::jsapi::detail
