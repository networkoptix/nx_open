#include "resource_tree_model_layout_node.h"

#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_context.h>

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

    connect(qnResourceAccessProvider, &QnResourceAccessProvider::accessChanged, this,
        &QnResourceTreeModelLayoutNode::handleAccessChanged);

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
    /* Updating node may cause it to be deleted. */
    auto guard = toSharedPointer();

    update();
    for (auto item: m_items)
        item->update();
}

QnResourceAccessSubject QnResourceTreeModelLayoutNode::getOwner() const
{
    if (type() == Qn::ResourceNode)
        return context()->user();

    auto owner = parent();
    if (owner && owner->type() == Qn::SharedLayoutsNode)
        owner = owner->parent();

    NX_ASSERT(owner);
    if (owner)
    {
        switch (owner->type())
        {
            /* Layout under user. */
            case Qn::ResourceNode:
                return owner->resource().dynamicCast<QnUserResource>();

            case Qn::RoleNode:
                return qnUserRolesManager->userRole(owner->uuid());

            default:
                NX_ASSERT(false);
                break;
        }
    }

    return QnResourceAccessSubject();
}

QIcon QnResourceTreeModelLayoutNode::iconBySubject(const QnResourceAccessSubject& subject) const
{
    switch (qnResourceAccessProvider->accessibleVia(subject, resource()))
    {
        case QnAbstractResourceAccessProvider::Source::videowall:
            return qnResIconCache->icon(QnResourceIconCache::VideoWallLayout);
        default:
            break;
    }
    return base_type::calculateIcon();
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

void QnResourceTreeModelLayoutNode::handleAccessChanged(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource)
{
    if (resource != this->resource())
        return;

    if (subject != getOwner())
        return;

    update();
}

void QnResourceTreeModelLayoutNode::handlePermissionsChanged(const QnResourcePtr& resource)
{
    /* Updating node may cause it to be deleted. */
    auto guard = toSharedPointer();

    base_type::handlePermissionsChanged(resource);
    for (auto item : m_items)
    {
        if (item->resource() == resource)
            item->update();
    }
}

QIcon QnResourceTreeModelLayoutNode::calculateIcon() const
{
    /* Check if node is in process of removing. */
    if (!resource())
        return QIcon();

    if (resource()->hasFlags(Qn::exported_layout))
        return base_type::calculateIcon();

    switch (type())
    {
        case Qn::ResourceNode:
        case Qn::SharedLayoutNode:
            return iconBySubject(getOwner());
        default:
            NX_ASSERT(false);
            break;
    }
    return base_type::calculateIcon();
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
