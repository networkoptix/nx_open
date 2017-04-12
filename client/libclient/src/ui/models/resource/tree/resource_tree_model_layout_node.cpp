#include "resource_tree_model_layout_node.h"

#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/tree/resource_tree_model_layout_node_manager.h>
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
}

QnResourceTreeModelLayoutNode::~QnResourceTreeModelLayoutNode()
{
}

QnResourceTreeModelNodeManager* QnResourceTreeModelLayoutNode::manager() const
{
    return model()->layoutNodeManager();
}

void QnResourceTreeModelLayoutNode::updateRecursive()
{
    update();
    for (auto item: m_items)
        item->update();
}

void QnResourceTreeModelLayoutNode::initialize()
{
    base_type::initialize();
    auto layout = resource().dynamicCast<QnLayoutResource>();
    NX_ASSERT(layout);
    if (!layout)
        return;

    for (const auto& item: layout->getItems())
        itemAdded(item);
}

void QnResourceTreeModelLayoutNode::deinitialize()
{
    for (auto node: m_items)
        node->deinitialize();

    m_items.clear();
    m_loadedItems = 0;
    updateLoadedState();

    base_type::deinitialize();
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
                return userRolesManager()->userRole(owner->uuid());

            default:
                NX_ASSERT(false);
                break;
        }
    }

    return QnResourceAccessSubject();
}

void QnResourceTreeModelLayoutNode::updateItem(const QnUuid& item)
{
    const auto iter = m_items.find(item);
    if (iter != m_items.end())
        (*iter)->update();
    else
        NX_ASSERT(false);
}

QIcon QnResourceTreeModelLayoutNode::calculateIcon() const
{
    /* Check if node is in process of removing. */
    if (!resource())
        return QIcon();

    return base_type::calculateIcon();
}

void QnResourceTreeModelLayoutNode::itemAdded(const QnLayoutItemData& item)
{
    NX_ASSERT(model());
    if (!model())
        return;

    if (item.uuid.isNull())
        return;

    QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(model(), item.uuid,
        Qn::LayoutItemNode));
    node->initialize();
    node->setParent(toSharedPointer());

    m_items.insert(item.uuid, node);

    const auto resource = resourcePool()->getResourceByDescriptor(item.resource);
    if (!resource)
        return;

    node->setResource(resource);

    ++m_loadedItems;
    updateLoadedState();
}

void QnResourceTreeModelLayoutNode::itemRemoved(const QnLayoutItemData& item)
{
    if (auto node = m_items.take(item.uuid))
    {
        if (node->resource())
            --m_loadedItems;
        node->deinitialize();
        updateLoadedState();
    }
}

void QnResourceTreeModelLayoutNode::updateItemResource(
    const QnUuid& item,
    const QnResourcePtr& resource)
{
    const auto iter = m_items.find(item);
    if (iter == m_items.end() || !(*iter)->resource().isNull() || resource.isNull())
    {
        NX_ASSERT(false);
        return;
    }

    (*iter)->setResource(resource);

    ++m_loadedItems;
    updateLoadedState();
}

bool QnResourceTreeModelLayoutNode::itemsLoaded() const
{
    return m_loadedItems == m_items.size();
}

void QnResourceTreeModelLayoutNode::updateLoadedState()
{
    NX_EXPECT(m_loadedItems >= 0 && m_loadedItems <= m_items.size());
    const bool loaded = itemsLoaded();

    if (loaded == m_loaded)
        return;

    m_loaded = loaded;

    if (!isPrimary())
        return;

    auto layoutNodeManager = qobject_cast<QnResourceTreeModelLayoutNodeManager*>(manager());
    NX_EXPECT(layoutNodeManager);
    layoutNodeManager->loadedStateChanged(this, loaded);
}
