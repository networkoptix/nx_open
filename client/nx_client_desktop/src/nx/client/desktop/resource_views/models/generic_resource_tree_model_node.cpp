#include "generic_resource_tree_model_node.h"

#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/tree/resource_tree_model_recorder_node.h>
#include <ui/workbench/workbench_context.h>

namespace nx {
namespace client {
namespace desktop {

GenericResourceTreeModelNode::GenericResourceTreeModelNode(
    QnResourceTreeModel* model,
    const IsAcceptableResourceCheckFunction& checkFunction,
    Qn::NodeType nodeType)
    :
    base_type(model, nodeType),
    m_isAcceptableCheck(checkFunction),
    m_type(nodeType)
{
    connect(resourceAccessProvider(), &QnResourceAccessProvider::accessChanged, this,
        &GenericResourceTreeModelNode::handleAccessChanged);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &GenericResourceTreeModelNode::rebuild);

    connect(context()->resourcePool(), &QnResourcePool::resourceAdded,
        this, &GenericResourceTreeModelNode::tryEnsureResourceNode);
    connect(context()->resourcePool(), &QnResourcePool::resourceRemoved,
        this, &GenericResourceTreeModelNode::tryRemoveResource);

    rebuild();
}

GenericResourceTreeModelNode::~GenericResourceTreeModelNode()
{
    /* Make sure all nodes are removed from the parent model. */
    clean();
}

void GenericResourceTreeModelNode::initialize()
{
    base_type::initialize();
    rebuild();
}

void GenericResourceTreeModelNode::deinitialize()
{
    disconnect(resourceAccessProvider(), nullptr, this, nullptr);
    disconnect(context(), nullptr, this, nullptr);
    clean();
    base_type::deinitialize();
}

void GenericResourceTreeModelNode::tryRemoveResource(const QnResourcePtr& resource)
{
    const auto it = std::find_if(m_items.begin(), m_items.end(),
        [resource](const QnResourceTreeModelNodePtr& node)
        {
            return node->resource()->getId() == resource->getId();
        });

    if (it != m_items.end())
        removeNode(*it);
}

void GenericResourceTreeModelNode::handleAccessChanged(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource)
{
    if (!context()->user() || subject.user() != context()->user())
        return;

    if (resourceAccessProvider()->hasAccess(subject, resource))
    {
        if (const auto node = tryEnsureResourceNode(resource))
            node->update();
    }
    else
    {
        for (auto node: m_items)
        {
            if (node->resource() != resource)
                continue;

            removeNode(node);
            cleanupRecorders();
            break;
        }
    }
}

QnResourceTreeModelNodePtr GenericResourceTreeModelNode::tryEnsureResourceNode(
    const QnResourcePtr& resource)
{
    if (!m_isAcceptableCheck(model(), resource))
        return QnResourceTreeModelNodePtr();

    for (auto node: m_items)
    {
        if (node->resource() == resource)
            return node;
    }

    QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(model(), resource,
        Qn::SharedResourceNode));
    node->initialize();

    auto parent = toSharedPointer();

    /* Create recorder nodes for cameras. */
    if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        QString groupId = camera->getGroupId();
        if (!groupId.isEmpty())
            parent = ensureRecorderNode(camera);
    }

    node->setParent(parent);
    m_items.append(node);
    return node;
}

QnResourceTreeModelNodePtr GenericResourceTreeModelNode::ensureRecorderNode(
    const QnVirtualCameraResourcePtr& camera)
{
    auto parent = toSharedPointer();
    auto id = camera->getGroupId();

    auto pos = m_recorders.find(id);
    if (pos == m_recorders.end())
    {
        // TODO: #GDM move to factory
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelRecorderNode(model(), camera));
        node->initialize();
        node->setParent(parent);

        pos = m_recorders.insert(id, node);
    }
    return *pos;
}

void GenericResourceTreeModelNode::rebuild()
{
    clean();

    if (!context()->user())
        return;

    for (const auto& resource: resourcePool()->getResources())
        tryEnsureResourceNode(resource);
}

void GenericResourceTreeModelNode::removeNode(const QnResourceTreeModelNodePtr& node)
{
    if (node->type() == Qn::RecorderNode)
    {
        /* Check if node was already removed. */
        auto key = m_recorders.key(node);
        if (key.isEmpty())
            return;

        /* Recursively remove all child nodes. */
        for (auto child: node->children())
            removeNode(child);

        m_recorders.remove(key);
    }
    else
    {
        /* Check if node was already removed. */
        if (!m_items.contains(node))
            return;

        m_items.removeOne(node);
    }

    node->deinitialize();
}

void GenericResourceTreeModelNode::clean()
{
    for (auto node: m_items)
        node->deinitialize();
    m_items.clear();

    for (auto node: m_recorders.values())
        node->deinitialize();
    m_recorders.clear();
}

void GenericResourceTreeModelNode::cleanupRecorders()
{
    QList<QnResourceTreeModelNodePtr> nodesToDelete;
    for (auto node: m_recorders.values())
    {
        if (node->children().isEmpty())
            nodesToDelete << node;
    }

    for (auto node: nodesToDelete)
        removeNode(node);
}

} // namespace desktop
} // namespace client
} // namespace nx

