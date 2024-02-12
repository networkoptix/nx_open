// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_layout_resource_index.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

UserLayoutResourceIndex::UserLayoutResourceIndex(const QnResourcePool* resourcePool):
    base_type(),
    m_resourcePool(resourcePool)
{
    blockSignals(true);
    indexAllLayouts();
    blockSignals(false);

    connect(m_resourcePool, &QnResourcePool::resourceAdded,
        this, &UserLayoutResourceIndex::onResourceAdded);

    connect(m_resourcePool, &QnResourcePool::resourceRemoved,
        this, &UserLayoutResourceIndex::onResourceRemoved);
}

QVector<QnResourcePtr> UserLayoutResourceIndex::layoutsWithParentUserId(const nx::Uuid& parentId) const
{
    const auto layoutSet = m_layoutsByParentUserId.value(parentId);
    return QVector<QnResourcePtr>(layoutSet.cbegin(), layoutSet.cend());
}

void UserLayoutResourceIndex::indexAllLayouts()
{
    const QnResourceList layouts = m_resourcePool->getResourcesWithFlag(Qn::layout);
    for (const auto& layout: layouts)
    {
        if (layout->hasFlags(Qn::exported) || layout->hasFlags(Qn::removed))
            continue;
        indexLayout(layout);
    }
}

void UserLayoutResourceIndex::onResourceAdded(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::layout))
        return;

    if (resource->hasFlags(Qn::exported) || resource->hasFlags(Qn::removed))
        return;

    indexLayout(resource);
}

void UserLayoutResourceIndex::onResourceRemoved(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::layout) || resource->hasFlags(Qn::exported))
        return;

    const nx::Uuid parentId = resource->getParentId();
    m_layoutsByParentUserId[parentId].remove(resource);
    emit layoutRemoved(resource, parentId);
}

void UserLayoutResourceIndex::onLayoutParentIdChanged(
    const QnResourcePtr& layout,
    const nx::Uuid& previousParentId)
{
    m_layoutsByParentUserId[previousParentId].remove(layout);
    emit layoutRemoved(layout, previousParentId);

    const auto parentId = layout->getParentId();
    m_layoutsByParentUserId[parentId].insert(layout);
    emit layoutAdded(layout, parentId);
}

void UserLayoutResourceIndex::indexLayout(const QnResourcePtr& layout)
{
    QnLayoutResourcePtr layoutResource = layout.dynamicCast<QnLayoutResource>();

    if (!layoutResource
        || layoutResource->getParentResource().dynamicCast<QnUserResource>().isNull())
    {
        return;
    }

    connect(layout.get(), &QnResource::parentIdChanged,
        this, &UserLayoutResourceIndex::onLayoutParentIdChanged);

    const nx::Uuid parentId = layout->getParentId();
    m_layoutsByParentUserId[parentId].insert(layout);
    emit layoutAdded(layout, parentId);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
