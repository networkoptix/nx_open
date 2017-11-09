#include "resource_tree_model_node_manager.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/resource_tree_model_node.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

QnResourceTreeModelNodeManager::QnResourceTreeModelNodeManager(QnResourceTreeModel* model):
    base_type(model),
    QnWorkbenchContextAware(model)
{
    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged, this,
        [this](const QnResourcePtr& resource)
        {
            resourceChainCall(resource, &QnResourceTreeModelNode::handlePermissionsChanged);
        });
}

QnResourceTreeModelNodeManager::~QnResourceTreeModelNodeManager()
{
}

void QnResourceTreeModelNodeManager::primaryNodeAdded(QnResourceTreeModelNode* node)
{
    const auto resource = node->resource();
    NX_ASSERT(resource);

    /* Receiver is node, not the manager! */
    /* So we can batch disconnect later. */

    const auto chainUpdate = [node]() { chainCall(node, &QnResourceTreeModelNode::update); };

    connect(resource, &QnResource::nameChanged,   node, chainUpdate);
    connect(resource, &QnResource::urlChanged,    node, chainUpdate);
    connect(resource, &QnResource::flagsChanged,  node, chainUpdate);
    connect(resource, &QnResource::parentIdChanged,  node, chainUpdate);

    connect(resource, &QnResource::statusChanged, node,
        [node]() { chainCall(node, &QnResourceTreeModelNode::updateResourceStatus); });

    if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        connect(camera, &QnVirtualCameraResource::statusFlagsChanged, node, chainUpdate);
        connect(camera, &QnVirtualCameraResource::capabilitiesChanged, node,
            [node]() { chainCall(node, &QnResourceTreeModelNode::updateResourceStatus); });
    }
}

void QnResourceTreeModelNodeManager::primaryNodeRemoved(QnResourceTreeModelNode* node)
{
    const auto resource = node->resource();
    NX_ASSERT(resource);

    resource->disconnect(node);
}

const QnResourceTreeModelNodeManager::PrimaryResourceNodes&
    QnResourceTreeModelNodeManager::primaryResourceNodes() const
{
    return m_primaryResourceNodes;
}

QnResourceTreeModelNodePtr QnResourceTreeModelNodeManager::nodeForResource(
    const QnResourcePtr& resource) const
{
    const auto iter = m_primaryResourceNodes.find(resource);
    return (iter != m_primaryResourceNodes.end() ? *iter : QnResourceTreeModelNodePtr());
}

void QnResourceTreeModelNodeManager::addResourceNode(const QnResourceTreeModelNodePtr& node)
{
    const auto resource = node->resource();
    if (!resource)
        return;

    NX_ASSERT(node->m_initialized);
    NX_ASSERT(node->m_next.isNull() && node->m_prev.isNull());

    auto& primary = m_primaryResourceNodes[resource];

    /* The node will be secondary: */
    if (primary)
    {
        /* Just insert it into the chain after the primary node. */
        node->m_prev = primary;
        node->m_next = primary->m_next;
        if (node->m_next)
            node->m_next->m_prev = node;
        primary->m_next = node;
    }
    /* The node will be primary: */
    else
    {
        primary = node;
        if (resource->resourcePool())
            primaryNodeAdded(node.data());
        else
            NX_EXPECT(false);
    }
}

void QnResourceTreeModelNodeManager::removeResourceNode(const QnResourceTreeModelNodePtr& node)
{
    const auto resource = node->resource();
    if (!resource)
        return;

    NX_ASSERT(node->m_initialized);

    /* The node was primary: */
    if (!node->m_prev)
    {
        NX_ASSERT(nodeForResource(resource) == node);
        primaryNodeRemoved(node.data());

        if (!node->m_next)
        {
            /* No more primary node for this resource. */
            m_primaryResourceNodes.remove(resource);
        }
        else
        {
            /* Next node becomes primary. */
            auto newPrimary = node->m_next;
            m_primaryResourceNodes[resource] = newPrimary;
            newPrimary->m_prev.clear();
            node->m_next.clear();

            if (resource->resourcePool())
                primaryNodeAdded(newPrimary.data());
        }
    }
    /* The node was secondary: */
    else
    {
        /* Just remove it from the chain. */
        node->m_prev->m_next = node->m_next;
        if (node->m_next)
        {
            node->m_next->m_prev = node->m_prev;
            node->m_next.clear();
        }
        node->m_prev.clear();
    }
}

