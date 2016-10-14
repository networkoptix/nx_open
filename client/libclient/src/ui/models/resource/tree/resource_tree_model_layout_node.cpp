#include "resource_tree_model_layout_node.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

QnResourceTreeModelLayoutNode::QnResourceTreeModelLayoutNode(
    QnResourceTreeModel* model,
    const QnResourcePtr& resource,
    Qn::NodeType nodeType)
    :
    base_type(model, resource, nodeType)
{
    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    NX_ASSERT(layout);
    if (!layout)
        return;

    connect(qnResPool, &QnResourcePool::resourceAdded, this,
        &QnResourceTreeModelLayoutNode::handleResourceAdded);

    connect(snapshotManager(), &QnWorkbenchLayoutSnapshotManager::flagsChanged, this,
        &QnResourceTreeModelLayoutNode::at_snapshotManager_flagsChanged);

    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged, this,
        &QnResourceTreeModelLayoutNode::handlePermissionsChanged);

    connect(layout, &QnResource::parentIdChanged, this,
        &QnResourceTreeModelLayoutNode::update);
    connect(layout, &QnLayoutResource::itemAdded, this,
        &QnResourceTreeModelLayoutNode::at_layout_itemAdded);
    connect(layout, &QnLayoutResource::itemRemoved, this,
        &QnResourceTreeModelLayoutNode::at_layout_itemRemoved);

    for (const auto& item : layout->getItems())
        at_layout_itemAdded(layout, item);
}

QnResourceTreeModelLayoutNode::~QnResourceTreeModelLayoutNode()
{
}

void QnResourceTreeModelLayoutNode::setResource(const QnResourcePtr &resource)
{
    NX_ASSERT(!resource);
    if (!this->resource())
        return;

    disconnect(qnResPool, nullptr, this, nullptr);
    base_type::setResource(resource);
    for (auto node : m_items)
        removeNode(node);
    m_items.clear();
}

void QnResourceTreeModelLayoutNode::setParent(const QnResourceTreeModelNodePtr& parent)
{
    base_type::setParent(parent);
    for (auto item: m_items)
        item->setParent(toSharedPointer());
}

void QnResourceTreeModelLayoutNode::updateRecursive()
{
    update();
    for (auto item: m_items)
        item->update();
}

void QnResourceTreeModelLayoutNode::removeNode(const QnResourceTreeModelNodePtr& node)
{
    node->setResource(QnResourcePtr());
    node->setParent(QnResourceTreeModelNodePtr());
}

void QnResourceTreeModelLayoutNode::handleResourceAdded(const QnResourcePtr& resource)
{
    /* There is a possible situation when layout is ready before its item's resource. */

    auto id = resource->getId();
    auto uniqueId = resource->getUniqueId();
    auto layout = this->resource().dynamicCast<QnLayoutResource>();
    NX_ASSERT(layout);

    for (auto node : m_items)
    {
        if (node->resource())
            continue;

        auto descriptor = layout->getItem(node->uuid()).resource;
        bool fit = descriptor.id.isNull()
            ? descriptor.uniqueId == uniqueId
            : descriptor.id == id;

        if (fit)
            node->setResource(resource);
    }
}

void QnResourceTreeModelLayoutNode::handlePermissionsChanged(const QnResourcePtr& resource)
{
    base_type::handlePermissionsChanged(resource);
    for (auto item : m_items)
    {
        if (item->resource() == resource)
            item->update();
    }
}

void QnResourceTreeModelLayoutNode::at_layout_itemAdded(const QnLayoutResourcePtr& /*layout*/,
    const QnLayoutItemData& item)
{
    NX_ASSERT(model());
    if (!model())
        return;

    QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(model(), item.uuid));
    node->setParent(toSharedPointer());

    auto resource = qnResPool->getResourceByDescriptor(item.resource);
    node->setResource(resource);

    m_items.insert(item.uuid, node);
}

void QnResourceTreeModelLayoutNode::at_layout_itemRemoved(const QnLayoutResourcePtr& /*layout*/,
    const QnLayoutItemData& item)
{
    if (auto node = m_items.take(item.uuid))
        removeNode(node);
}

void QnResourceTreeModelLayoutNode::at_snapshotManager_flagsChanged(
    const QnLayoutResourcePtr& layout)
{
    if (layout != resource())
        return;

    setModified(snapshotManager()->isModified(layout));
}
