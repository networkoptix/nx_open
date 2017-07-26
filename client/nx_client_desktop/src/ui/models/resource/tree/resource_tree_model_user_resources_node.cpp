#include "resource_tree_model_user_resources_node.h"

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/tree/resource_tree_model_recorder_node.h>
#include <ui/workbench/workbench_context.h>

QnResourceTreeModelUserResourcesNode::QnResourceTreeModelUserResourcesNode(
    QnResourceTreeModel* model)
    :
    base_type(model, Qn::UserResourcesNode)
{
    connect(resourceAccessProvider(), &QnResourceAccessProvider::accessChanged, this,
        &QnResourceTreeModelUserResourcesNode::handleAccessChanged);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnResourceTreeModelUserResourcesNode::rebuild);
}

QnResourceTreeModelUserResourcesNode::~QnResourceTreeModelUserResourcesNode()
{
    /* Make sure all nodes are removed from the parent model. */
    clean();
}

void QnResourceTreeModelUserResourcesNode::initialize()
{
    base_type::initialize();
    rebuild();
}

void QnResourceTreeModelUserResourcesNode::deinitialize()
{
    disconnect(resourceAccessProvider(), nullptr, this, nullptr);
    disconnect(context(), nullptr, this, nullptr);
    clean();
    base_type::deinitialize();
}

void QnResourceTreeModelUserResourcesNode::handleAccessChanged(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource)
{
    if (!context()->user() || subject.user() != context()->user())
        return;

    if (resourceAccessProvider()->hasAccess(subject, resource))
    {
        if (isResourceVisible(resource))
            ensureResourceNode(resource)->update();
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

bool QnResourceTreeModelUserResourcesNode::isResourceVisible(const QnResourcePtr& resource) const
{
    if (model()->scope() == QnResourceTreeModel::UsersScope)
        return false;

    if (!QnResourceAccessFilter::isShareableMedia(resource))
        return false;

    /* Web pages are displayed under separate node. */
    if (resource->hasFlags(Qn::web_page))
        return false;

    if (!resourceAccessProvider()->hasAccess(context()->user(), resource))
        return false;

    if (model()->scope() == QnResourceTreeModel::CamerasScope)
        return resource->hasFlags(Qn::live_cam);

    return true;
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserResourcesNode::ensureResourceNode(
    const QnResourcePtr& resource)
{
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

QnResourceTreeModelNodePtr QnResourceTreeModelUserResourcesNode::ensureRecorderNode(
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

void QnResourceTreeModelUserResourcesNode::rebuild()
{
    clean();

    if (!context()->user())
        return;

    for (const auto& resource: resourcePool()->getResources())
    {
        if (isResourceVisible(resource))
            ensureResourceNode(resource);
    }
}

void QnResourceTreeModelUserResourcesNode::removeNode(const QnResourceTreeModelNodePtr& node)
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

void QnResourceTreeModelUserResourcesNode::clean()
{
    for (auto node: m_items)
        node->deinitialize();
    m_items.clear();

    for (auto node: m_recorders.values())
        node->deinitialize();
    m_recorders.clear();
}

void QnResourceTreeModelUserResourcesNode::cleanupRecorders()
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
