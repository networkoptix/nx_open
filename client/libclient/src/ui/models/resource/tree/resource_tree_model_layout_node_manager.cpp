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
    connect(snapshotManager(), &QnWorkbenchLayoutSnapshotManager::flagsChanged, this,
        [this](const QnLayoutResourcePtr& layout)
        {
            resourceChainCall(layout, &QnResourceTreeModelNode::setModified,
                snapshotManager()->isModified(layout));
        });

    connect(qnResourceAccessProvider, &QnResourceAccessProvider::accessChanged, this,
        [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
        {
            resourceChainCall<QnResourceTreeModelLayoutNode>(resource,
                &QnResourceTreeModelLayoutNode::handleAccessChanged,
                subject);
        });

    connect(qnResPool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            for (const auto& primaryLayoutNode: m_primaryResourceNodes)
            {
                chainCall<QnResourceTreeModelLayoutNode>(primaryLayoutNode,
                    &QnResourceTreeModelLayoutNode::handleResourceAdded,
                    resource);
            }
        });

    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged, this,
        [this](const QnResourcePtr& resource)
        {
            for (const auto& primaryLayoutNode: m_primaryResourceNodes)
            {
                chainCall<QnResourceTreeModelLayoutNode>(primaryLayoutNode,
                    &QnResourceTreeModelLayoutNode::handlePermissionsChanged,
                    resource);
            }
        });
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
