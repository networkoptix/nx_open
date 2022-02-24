// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_resource_index.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

LayoutResourceIndex::LayoutResourceIndex(const QnResourcePool* resourcePool):
    base_type(),
    m_resourcePool(resourcePool)
{
    blockSignals(true);
    indexAllLayouts();
    blockSignals(false);

    connect(m_resourcePool, &QnResourcePool::resourceAdded,
        this, &LayoutResourceIndex::onResourceAdded);

    connect(m_resourcePool, &QnResourcePool::resourceRemoved,
        this, &LayoutResourceIndex::onResourceRemoved);
}

QVector<QnResourcePtr> LayoutResourceIndex::allLayouts() const
{
    QVector<QnResourcePtr> result;
    for (const auto& layoutsSet: m_layoutsByParentId)
        std::copy(layoutsSet.cbegin(), layoutsSet.cend(), std::back_inserter(result));
    return result;
}

QVector<QnResourcePtr> LayoutResourceIndex::layoutsWithParentId(const QnUuid& parentId) const
{
    const auto layoutSet = m_layoutsByParentId.value(parentId);
    return QVector<QnResourcePtr>(layoutSet.cbegin(), layoutSet.cend());
}

void LayoutResourceIndex::indexAllLayouts()
{
    const QnResourceList layouts = m_resourcePool->getResourcesWithFlag(Qn::layout);
    for (const auto& layout: layouts)
    {
        if (layout->hasFlags(Qn::exported) || layout->hasFlags(Qn::removed))
            continue;
        indexLayout(layout);
    }
}

void LayoutResourceIndex::onResourceAdded(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::layout))
        return;

    if (resource->hasFlags(Qn::exported) || resource->hasFlags(Qn::removed))
        return;

    indexLayout(resource);
}

void LayoutResourceIndex::onResourceRemoved(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::layout) || resource->hasFlags(Qn::exported))
        return;

    const QnUuid parentId = resource->getParentId();
    m_layoutsByParentId[parentId].remove(resource);
    emit layoutRemoved(resource, parentId);
}

void LayoutResourceIndex::onLayoutParentIdChanged(
    const QnResourcePtr& layout,
    const QnUuid& previousParentId)
{
    m_layoutsByParentId[previousParentId].remove(layout);
    emit layoutRemoved(layout, previousParentId);

    const auto parentId = layout->getParentId();
    m_layoutsByParentId[parentId].insert(layout);
    emit layoutAdded(layout, parentId);
}

void LayoutResourceIndex::indexLayout(const QnResourcePtr& layout)
{
    const QnUuid parentId = layout->getParentId();

    m_layoutsByParentId[parentId].insert(layout);

    connect(layout.get(), &QnResource::parentIdChanged,
        this, &LayoutResourceIndex::onLayoutParentIdChanged);

    emit layoutAdded(layout, layout->getParentId());
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
