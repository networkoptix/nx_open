// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_api_backend.h"

#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include "helpers.h"
#include "resources_structures.h"

namespace nx::vms::client::desktop::jsapi::detail {

struct ResourcesApiBackend::Private
{
    Private(
        SystemContext* context,
        ResourcesApiBackend* q);

    /**
     * Show if we currently support specific resource type in API and user has enough access
     * rights.
     */
    bool isResourceAvailable(const QnResourcePtr& resource) const;

    ResourcesApiBackend* const q;
    SystemContext* const context;
    QnResourcePool* const pool;
};

ResourcesApiBackend::Private::Private(
    SystemContext* context,
    ResourcesApiBackend* q)
    :
    q(q),
    context(context),
    pool(context->resourcePool())
{
}

bool ResourcesApiBackend::Private::isResourceAvailable(const QnResourcePtr& resource) const
{
    return resource
        && detail::resourceType(resource) != detail::ResourceType::undefined
        && ResourceAccessManager::hasPermissions(resource, Qn::ViewContentPermission);
}

//-------------------------------------------------------------------------------------------------

ResourcesApiBackend::ResourcesApiBackend(
    QnWorkbenchContext* context,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(context->systemContext(), this))
{
    connect(d->pool, &QnResourcePool::resourcesAdded, this,
        [this](const QnResourceList& resources)
        {
            const auto addedResources = resources.filtered(
                [this](const QnResourcePtr& resource) { return d->isResourceAvailable(resource); });

            for (const auto& resource: addedResources)
                emit added(detail::Resource::from(resource));
        });

    connect(d->pool, &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            // Do not check availability since after removal we don't have any permission
            // for deleted resource.
            for (const auto& resource: resources)
                emit removed(resource->getId());
        });
}

ResourcesApiBackend::~ResourcesApiBackend()
{
}

detail::Resource::List ResourcesApiBackend::resources() const
{
    const auto result = d->pool->getResources(
        [this](const QnResourcePtr& resource) { return d->isResourceAvailable(resource); });
    return detail::Resource::from(result);
}

detail::ResourceResult ResourcesApiBackend::resource(const QUuid& resourceId) const
{
    const auto resource = d->pool->getResourceById(resourceId);
    if (d->isResourceAvailable(resource))
        return detail::ResourceResult{Error::success(), detail::Resource::from(resource)};

    return detail::ResourceResult{
        Error::failed(tr("Resource is not available for the usage with JS API")), {}};
}

} // namespace nx::vms::client::desktop::jsapi::detail
