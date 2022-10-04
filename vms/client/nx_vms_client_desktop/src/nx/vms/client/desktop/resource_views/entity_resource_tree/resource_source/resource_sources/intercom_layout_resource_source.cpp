// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_layout_resource_source.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <client/client_globals.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
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
        if (m_intercomLocalLayouts.contains(resource))
        {
            m_intercomLocalLayouts.remove(resource);
            resource->disconnect(this);
            emit resourceRemoved(resource);
        }
        else if (resource->hasFlags(Qn::layout) && !resource->hasFlags(Qn::local))
        {
            LayoutResourcePtr layout = resource.dynamicCast<LayoutResource>();
            if (NX_ASSERT(layout) && layout->isIntercomLayout())
                emit resourceRemoved(resource);
        }
    }
}

bool IntercomLayoutResourceSource::processResource(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::layout))
        return false;

    if (resource->hasFlags(Qn::removed) || resource->hasFlags(Qn::exported))
        return false;

    if (resource->hasFlags(Qn::local_intercom_layout))
    {
        if (!m_intercomLocalLayouts.contains(resource))
        {
            // Handling Qn::local flag change on layout saving.
            connect(resource.get(), &QnResource::flagsChanged, this,
                [this](const QnResourcePtr& resource)
                {
                    if (!resource->hasFlags(Qn::local))
                    {
                        resource->disconnect(this);
                        m_intercomLocalLayouts.remove(resource);
                    }
                });

            m_intercomLocalLayouts.insert(resource);
        }

        return true;
    }

    const auto layout = resource.dynamicCast<LayoutResource>();
    return NX_ASSERT(layout) && layout->isIntercomLayout();
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
