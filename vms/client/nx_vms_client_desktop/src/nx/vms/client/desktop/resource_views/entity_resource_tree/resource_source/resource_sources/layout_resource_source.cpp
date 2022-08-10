// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_resource_source.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <client/client_globals.h>
#include <nx/vms/common/intercom/utils.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

LayoutResourceSource::LayoutResourceSource(
    const QnResourcePool* resourcePool,
    const QnUserResourcePtr& parentUser,
    bool includeShared)
    :
    base_type(),
    m_resourcePool(resourcePool),
    m_parentUser(parentUser),
    m_includeShared(includeShared)
{
    if (!NX_ASSERT(m_resourcePool))
        return;

    connect(m_resourcePool, &QnResourcePool::resourceAdded,
        this, &LayoutResourceSource::onResourceAdded);

    connect(m_resourcePool, &QnResourcePool::resourceRemoved,
        this, &LayoutResourceSource::onResourceRemoved);
}

QVector<QnResourcePtr> LayoutResourceSource::getResources()
{
    const auto resources = m_resourcePool->getResourcesWithFlag(Qn::layout);

    QVector<QnResourcePtr> result;
    for (const auto& resource: resources)
        processResource(resource, [&result, resource]() { result.push_back(resource); });

    return result;
}

void LayoutResourceSource::holdLocalLayout(const QnResourcePtr& layoutResource)
{
    m_localLayouts.insert(layoutResource);

    connect(layoutResource.get(), &QnResource::flagsChanged, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::remote))
            {
                m_localLayouts.remove(resource);
                resource->disconnect(this);
                onResourceAdded(resource);
            }
        });
}

void LayoutResourceSource::onResourceAdded(const QnResourcePtr& resource)
{
    processResource(resource, [this, resource]() { emit resourceAdded( resource ); });
}

void LayoutResourceSource::onResourceRemoved(const QnResourcePtr& resource)
{
    resource->disconnect(this);

    if (m_localLayouts.contains(resource))
        m_localLayouts.remove(resource);

    if (resource->hasFlags(Qn::layout))
    {
        if (!nx::vms::common::isIntercomLayout(resource))
            emit resourceRemoved(resource);
    }
}

void LayoutResourceSource::onLayoutParentIdChanged(
    const QnResourcePtr& layoutResource,
    const QnUuid& previousParentId)
{
    if (layoutResource->getParentId().isNull() && !layoutResource->hasFlags(Qn::removed)
        && m_includeShared)
    {
        emit resourceAdded(layoutResource);
    }
}

void LayoutResourceSource::processResource(
    const QnResourcePtr& resource,
    std::function<void()> successHandler)
{
    if (!resource->hasFlags(Qn::layout))
        return;

    if (resource->hasFlags(Qn::removed) || resource->hasFlags(Qn::exported))
        return;

    const auto layout = resource.staticCast<QnLayoutResource>();

    if (layout->isServiceLayout() || layout->data().contains(Qn::LayoutSearchStateRole))
        return;

    if (layout->hasFlags(Qn::local) && !layout->hasFlags(Qn::local_intercom_layout))
    {
        holdLocalLayout(resource);
        return;
    }

    connect(resource.get(), &QnResource::parentIdChanged,
        this, &LayoutResourceSource::onLayoutParentIdChanged);

    if (nx::vms::common::isIntercomLayout(layout))
        return;

    if (!m_parentUser
        || (layout->getParentResource() == m_parentUser)
        || (m_includeShared && layout->isShared()))
    {
        successHandler();
    }
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
