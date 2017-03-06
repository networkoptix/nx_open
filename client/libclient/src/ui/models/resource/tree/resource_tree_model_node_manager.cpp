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

    const auto chainUpdate = [node]() { chainCall(node, &QnResourceTreeModelNode::update); };

    connect(resource, &QnResource::nameChanged,  this, chainUpdate);
    connect(resource, &QnResource::urlChanged,   this, chainUpdate);
    connect(resource, &QnResource::flagsChanged, this, chainUpdate);

    connect(resource, &QnResource::statusChanged, this,
        [node]() { chainCall(node, &QnResourceTreeModelNode::updateResourceStatus); });

    if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        connect(camera, &QnVirtualCameraResource::statusFlagsChanged, this, chainUpdate);
}

void QnResourceTreeModelNodeManager::primaryNodeRemoved(QnResourceTreeModelNode* node)
{
    const auto resource = node->resource();
    NX_ASSERT(resource);

    resource->disconnect(this);
}

void QnResourceTreeModelNodeManager::addResourceNode(QnResourceTreeModelNode* node)
{
    const auto resource = node->resource();
    if (!resource)
        return;

    auto primary = m_primaryResourceNodes[resource];

    /* The node will be secondary: */
    if (primary)
    {
        /* Just insert it into the chain after the primary node. */
        node->m_prev = primary;
        node->m_next = primary->m_next;
        primary->m_next = node;
    }
    /* The node will be primary: */
    else
    {
        m_primaryResourceNodes[resource] = node;
        if (resource->resourcePool())
            primaryNodeAdded(node);
        else
            NX_EXPECT(false);
    }
}

void QnResourceTreeModelNodeManager::removeResourceNode(QnResourceTreeModelNode* node)
{
    const auto resource = node->resource();
    if (!resource)
        return;

    /* The node was primary: */
    if (!node->m_prev)
    {
        NX_ASSERT(m_primaryResourceNodes[resource] == node);
        primaryNodeRemoved(node);

        if (!node->m_next)
        {
            /* No more primary node for this resource. */
            m_primaryResourceNodes.remove(resource);
        }
        else
        {
            /* Next node becomes primary. */
            m_primaryResourceNodes[resource] = node->m_next;
            if (resource->resourcePool())
                primaryNodeAdded(node->m_next);
            node->m_next->m_prev = nullptr;
            node->m_next = nullptr;
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
            node->m_next = nullptr;
        }
        node->m_prev = nullptr;
    }
}

