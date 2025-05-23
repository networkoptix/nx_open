// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_layout_resource_source.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <client/client_globals.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/common/intercom/utils.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

IntercomLayoutResourceSource::IntercomLayoutResourceSource(const QnResourcePool* resourcePool):
    base_type(),
    m_resourcePool(resourcePool)
{
    if (!NX_ASSERT(m_resourcePool))
        return;

    connect(m_resourcePool, &QnResourcePool::resourcesAdded,
        this, &IntercomLayoutResourceSource::onResourcesAdded);

    connect(m_resourcePool, &QnResourcePool::resourcesRemoved,
        this, &IntercomLayoutResourceSource::onResourcesRemoved);
}

QVector<QnResourcePtr> IntercomLayoutResourceSource::getResources()
{
    const auto resources = m_resourcePool->getResourcesWithFlag(Qn::layout);

    QVector<QnResourcePtr> result;
    for (const auto& resource: resources)
    {
        if (processResource(resource))
            result.push_back(resource);
    }

    return result;
}

void IntercomLayoutResourceSource::onResourcesAdded(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        if (processResource(resource))
            emit resourceAdded(resource);
    }
}

void IntercomLayoutResourceSource::onResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        if (!resource->hasFlags(Qn::layout))
            continue;

        resource->disconnect(this);

        core::LayoutResourcePtr layout = resource.objectCast<core::LayoutResource>();
        if (NX_ASSERT(layout) && layout->layoutType() == core::LayoutResource::LayoutType::intercom)
            emit resourceRemoved(resource);
    }
}

void IntercomLayoutResourceSource::onLayoutTypeChanged(const QnResourcePtr& resource)
{
    if (processResource(resource))
        emit resourceAdded(resource);
}

bool IntercomLayoutResourceSource::processResource(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::layout))
        return false;

    if (resource->hasFlags(Qn::removed) || resource->hasFlags(Qn::exported))
        return false;

    const auto layout = resource.objectCast<core::LayoutResource>();
    if (!NX_ASSERT(layout))
        return false;

    if (layout->layoutType() == core::LayoutResource::LayoutType::intercom)
        return true;

    connect(layout.get(), &core::LayoutResource::layoutTypeChanged, this,
        &IntercomLayoutResourceSource::onLayoutTypeChanged, Qt::UniqueConnection);

    return false;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
