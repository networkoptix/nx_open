#include "resource_tree_model_layout_node_manager.h"

#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/tree/resource_tree_model_layout_node.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

QnResourceTreeModelLayoutNodeManager::QnResourceTreeModelLayoutNodeManager(QnResourceTreeModel* model):
    base_type(model)
{
    connect(snapshotManager(), &QnWorkbenchLayoutSnapshotManager::layoutFlagsChanged, this,
        [this](const QnLayoutResourcePtr& layout)
        {
            resourceChainCall(layout, &QnResourceTreeModelNode::setModified,
                snapshotManager()->isModified(layout));
        });

    connect(resourceAccessProvider(), &QnResourceAccessProvider::accessChanged, this,
        [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
        {
            auto primary = nodeForResource(resource).objectCast<QnResourceTreeModelLayoutNode>();
            if (!primary)
                return;

            if (subject != primary->getOwner())
                return;

            chainCall(primary.data(), &QnResourceTreeModelNode::update);
        });

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnResourceTreeModelLayoutNodeManager::handleResourceAdded);
}

QnResourceTreeModelLayoutNodeManager::~QnResourceTreeModelLayoutNodeManager()
{
}

void QnResourceTreeModelLayoutNodeManager::primaryNodeAdded(QnResourceTreeModelNode* node)
{
    base_type::primaryNodeAdded(node);

    QnLayoutResourcePtr layout = node->resource().dynamicCast<QnLayoutResource>();
    NX_ASSERT(layout);
    if (!layout)
        return;

    connect(layout, &QnResource::parentIdChanged, node,
        &QnResourceTreeModelNode::update);

    connect(layout, &QnLayoutResource::itemAdded, node,
        [node](const QnLayoutResourcePtr& /*layout*/, const QnLayoutItemData& item)
        {
            chainCall<QnResourceTreeModelLayoutNode>(node,
                &QnResourceTreeModelLayoutNode::itemAdded, item);
        });

    connect(layout, &QnLayoutResource::itemRemoved, node,
        [node](const QnLayoutResourcePtr& /*layout*/, const QnLayoutItemData& item)
        {
            chainCall<QnResourceTreeModelLayoutNode>(node,
                &QnResourceTreeModelLayoutNode::itemRemoved, item);
        });
}

void QnResourceTreeModelLayoutNodeManager::handleResourceAdded(const QnResourcePtr& resource)
{
    /* There is a possible situation when layout is ready before its item's resource. */

    auto id = resource->getId();
    auto uniqueId = resource->getUniqueId();

    // Make a copy to avoid collision when modified inside of ::updateItemResource
    const auto loading = m_loadingLayouts;
    for (auto node: loading)
    {
        NX_EXPECT(!node->itemsLoaded());

        const auto layout = node->resource().dynamicCast<QnLayoutResource>();
        NX_ASSERT(layout);

        for (auto item: node->m_items)
        {
            if (item->resource())
                continue;

            const auto descriptor = layout->getItem(item->uuid()).resource;
            const bool fit = descriptor.id.isNull()
                ? descriptor.uniqueId == uniqueId
                : descriptor.id == id;

            if (!fit)
                continue;

            chainCall<QnResourceTreeModelLayoutNode>(node,
                &QnResourceTreeModelLayoutNode::updateItemResource,
                item->uuid(), resource);
        }
    }
}

void QnResourceTreeModelLayoutNodeManager::loadedStateChanged(
    QnResourceTreeModelLayoutNode* node, bool loaded)
{
    if (loaded)
        m_loadingLayouts.remove(node);
    else
        m_loadingLayouts.insert(node);
}
