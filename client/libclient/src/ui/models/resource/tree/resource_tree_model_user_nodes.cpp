#include "resource_tree_model_user_nodes.h"

#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <nx_ec/data/api_user_group_data.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/resource_tree_model_node.h>
#include <ui/models/resource/tree/resource_tree_model_layout_node.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

namespace {

bool isLayout(const QnResourcePtr& resource)
{
    return resource->hasFlags(Qn::layout);
}

//TODO: #GDM think where else can we place this check
bool isMediaResource(const QnResourcePtr& resource)
{
    if (resource->hasFlags(Qn::local))
        return false;

    if (resource->hasFlags(Qn::desktop_camera))
        return false;

    return resource->hasFlags(Qn::live_cam)
        || resource->hasFlags(Qn::web_page)
        || resource->hasFlags(Qn::server);
}

bool isUser(const QnResourcePtr& resource)
{
    return resource->hasFlags(Qn::user);
}

}

QnResourceTreeModelUserNodes::QnResourceTreeModelUserNodes(
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent, true),
    m_model(nullptr),
    m_rootNode(),
    m_allNodes(),
    m_roles(),
    m_users(),
    m_placeholders(),
    m_shared(),
    m_recorders()
{
    connect(qnResourceAccessProvider, &QnResourceAccessProvider::accessChanged, this,
        &QnResourceTreeModelUserNodes::handleAccessChanged);
    connect(qnGlobalPermissionsManager, &QnGlobalPermissionsManager::globalPermissionsChanged,
        this, &QnResourceTreeModelUserNodes::handleGlobalPermissionsChanged);

    connect(qnResPool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (auto user = resource.dynamicCast<QnUserResource>())
            {
                connect(user, &QnUserResource::enabledChanged, this,
                    &QnResourceTreeModelUserNodes::handleUserEnabledChanged);
            }
        });

    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            disconnect(resource, nullptr, this, nullptr);
        });
}

QnResourceTreeModelUserNodes::~QnResourceTreeModelUserNodes()
{
    /* Make sure all nodes are removed from the parent model. */
    clean();
}

QnResourceTreeModel* QnResourceTreeModelUserNodes::model() const
{
    return m_model;
}

void QnResourceTreeModelUserNodes::setModel(QnResourceTreeModel* value)
{
    m_model = value;
    initializeContext(m_model);
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::rootNode() const
{
    return m_rootNode;
}

void QnResourceTreeModelUserNodes::setRootNode(const QnResourceTreeModelNodePtr& node)
{
    m_rootNode = node;
}

void QnResourceTreeModelUserNodes::rebuild()
{
    clean();

    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
        return;

    for (const auto& role : qnUserRolesManager->userRoles())
        rebuildSubjectTree(role);

    for (const auto& user : qnResPool->getResources<QnUserResource>())
    {
        /* Current user moved to own node. */
        if (m_model->scope() == QnResourceTreeModel::FullScope)
        {
            if (user == context()->user())
                continue;
        }

        rebuildSubjectTree(user);
    }

}

QList<QnResourceTreeModelNodePtr> QnResourceTreeModelUserNodes::children(
    const QnResourceTreeModelNodePtr& node) const
{
    /* Calculating children this way because bastard nodes are absent in node's childred() list. */
    QList<QnResourceTreeModelNodePtr> result;
    for (auto existing : m_allNodes)
    {
        if (existing->parent() == node)
            result << existing;
    }
    return result;
}

QList<Qn::NodeType> QnResourceTreeModelUserNodes::allPlaceholders() const
{
    static QList<Qn::NodeType> placeholders
    {
        Qn::AllCamerasAccessNode,
        Qn::AllLayoutsAccessNode,
        Qn::SharedResourcesNode,
        Qn::SharedLayoutsNode,
        Qn::RoleUsersNode
    };
    return placeholders;
}

bool QnResourceTreeModelUserNodes::placeholderAllowedForSubject(
    const QnResourceAccessSubject& subject, Qn::NodeType nodeType) const
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return false;

    if (m_model->scope() != QnResourceTreeModel::FullScope)
        return false;

    /* Do not show user placeholders under groups. */
    if (subject.user() && subject.user()->role() == Qn::UserRole::CustomUserGroup)
        return false;

    bool isRole = !subject.user();
    bool hasAllMedia = qnGlobalPermissionsManager->hasGlobalPermission(subject,
        Qn::GlobalAccessAllMediaPermission);
    bool isAdmin = qnGlobalPermissionsManager->hasGlobalPermission(subject,
        Qn::GlobalAdminPermission);

    switch (nodeType)
    {
        /* 'All Cameras and Resources' visible to users with all media access. */
        case Qn::AllCamerasAccessNode:
            return hasAllMedia;

        /* 'All Shared Layouts' visible to admin only and never visible to roles. */
        case Qn::AllLayoutsAccessNode:
            return !isRole && isAdmin;

        /* If cameras set is limited, show 'Cameras' node and 'Layouts' node. */
        case Qn::SharedResourcesNode:
            return !hasAllMedia;

        /* 'Layouts' node also always visible to roles. */
        case Qn::SharedLayoutsNode:
            return isRole || !hasAllMedia;

        case Qn::RoleUsersNode:
            return isRole;

        default:
            break;
    }
    NX_ASSERT(false, "Should never get here");
    return false;
}

bool QnResourceTreeModelUserNodes::showLayoutForSubject(const QnResourceAccessSubject& subject,
    const QnLayoutResourcePtr& layout) const
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return false;

    if (m_model->scope() != QnResourceTreeModel::FullScope)
        return false;

    if (subject.user() && subject.user() == context()->user())
        return false;

    if (layout->isFile())
        return false;

    if (qnResPool->isAutoGeneratedLayout(layout))
        return false;

    if (layout->isShared())
    {
        /* Admins see all shared layouts. */
        if (qnGlobalPermissionsManager->hasGlobalPermission(subject, Qn::GlobalAdminPermission))
        {
            NX_ASSERT(subject.user());
            if (subject.user())
                return false;
        }

        /* Shared layouts are displayed under group node. */
        if (subject.user() && subject.user()->role() == Qn::UserRole::CustomUserGroup)
            return false;

        return qnResourceAccessProvider->hasAccess(subject, layout);
    }

    return subject.user()
        && layout->getParentId() == subject.id();
}

bool QnResourceTreeModelUserNodes::showMediaForSubject(const QnResourceAccessSubject& subject,
    const QnResourcePtr& media) const
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return false;

    if (m_model->scope() != QnResourceTreeModel::FullScope)
        return false;

    /* Hide media for users who has access to all media. */
    if (qnGlobalPermissionsManager->hasGlobalPermission(subject,
        Qn::GlobalAccessAllMediaPermission))
            return false;

    /* Shared resources are displayed under group node. */
    if (subject.user() && subject.user()->role() == Qn::UserRole::CustomUserGroup)
        return false;

    return qnResourceAccessProvider->hasAccess(subject, media);
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::ensureSubjectNode(
    const QnResourceAccessSubject& subject)
{
    NX_ASSERT(subject.isValid());

    if (subject.user())
        return ensureUserNode(subject.user());

    return ensureRoleNode(subject.role());
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::ensureRoleNode(
    const ec2::ApiUserGroupData& role)
{
    auto pos = m_roles.find(role.id);
    if (pos == m_roles.end())
    {
        NX_ASSERT(!role.isNull());

        QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(m_model, role.id,
            Qn::RoleNode));
        node->setParent(m_rootNode);
        m_allNodes.append(node);

        pos = m_roles.insert(role.id, node);
    }
    return *pos;
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::ensureUserNode(
    const QnUserResourcePtr& user)
{
    NX_ASSERT(user);

    auto id = user->getId();
    auto pos = m_users.find(id);
    if (pos == m_users.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(m_model, user,
            Qn::ResourceNode));

        auto parent = m_rootNode;
        if (user->role() == Qn::UserRole::CustomUserGroup)
        {
            auto role = qnUserRolesManager->userRole(user->userGroup());
            if (!role.isNull())
                parent = ensureRoleNode(role);
        }
        node->setParent(parent);
        m_allNodes.append(node);

        pos = m_users.insert(id, node);
    }
    return *pos;
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::ensureResourceNode(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
{
    if (isLayout(resource))
    {
        auto layout = resource.dynamicCast<QnLayoutResource>();
        NX_ASSERT(layout);
        if (layout && showLayoutForSubject(subject, layout))
            return ensureLayoutNode(subject, layout);
    }
    else if (isMediaResource(resource))
    {
        if (showMediaForSubject(subject, resource))
            return ensureMediaNode(subject, resource);
    }

    return QnResourceTreeModelNodePtr();
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::ensureLayoutNode(
    const QnResourceAccessSubject& subject, const QnLayoutResourcePtr& layout)
{
    auto& nodes = m_shared[subject.id()];

    for (auto node : nodes)
    {
        if (node->resource() == layout)
            return node;
    }

    auto nodeType = layout->isShared()
        ? Qn::SharedLayoutNode
        : Qn::ResourceNode;

    QnResourceTreeModelNodePtr node(new QnResourceTreeModelLayoutNode(m_model, layout, nodeType));
    auto parent = ensureSubjectNode(subject);
    if (placeholderAllowedForSubject(subject, Qn::SharedLayoutsNode))
        parent = ensurePlaceholderNode(subject, Qn::SharedLayoutsNode);

    node->setParent(parent);
    node->update();
    m_allNodes.append(node);
    nodes << node;
    return node;
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::ensureMediaNode(
    const QnResourceAccessSubject& subject, const QnResourcePtr& media)
{
    auto& nodes = m_shared[subject.id()];

    for (auto node : nodes)
    {
        if (node->resource() == media)
            return node;
    }

    QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(m_model, media,
        Qn::SharedResourceNode));

    auto parent = ensureSubjectNode(subject);
    if (placeholderAllowedForSubject(subject, Qn::SharedResourcesNode))
        parent = ensurePlaceholderNode(subject, Qn::SharedResourcesNode);

    /* Create recorder nodes for cameras. */
    if (auto camera = media.dynamicCast<QnVirtualCameraResource>())
    {
        QString groupId = camera->getGroupId();
        if (!groupId.isEmpty())
            parent = ensureRecorderNode(parent, groupId, camera->getGroupName());
    }

    node->setParent(parent);
    m_allNodes.append(node);
    nodes << node;
    return node;
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::ensurePlaceholderNode(
    const QnResourceAccessSubject& subject, Qn::NodeType nodeType)
{
    NX_ASSERT(subject.isValid());
    NX_ASSERT(placeholderAllowedForSubject(subject, nodeType));

    NodeList& placeholders = m_placeholders[subject.id()];
    for (auto node : placeholders)
    {
        if (node->type() == nodeType)
            return node;
    }

    QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(m_model, nodeType));
    node->setParent(ensureSubjectNode(subject));
    node->update();

    placeholders.push_back(node);
    m_allNodes.append(node);
    return node;
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::ensureRecorderNode(
    const QnResourceTreeModelNodePtr& parentNode,
    const QString& groupId,
    const QString& groupName)
{
    auto& recorders = m_recorders[parentNode];
    auto pos = recorders.find(groupId);
    if (pos == recorders.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(m_model, Qn::RecorderNode,
            !groupName.isEmpty() ? groupName : groupId));
        node->setParent(parentNode);

        pos = recorders.insert(groupId, node);
        m_allNodes.append(*pos);
    }
    return *pos;
}

void QnResourceTreeModelUserNodes::rebuildSubjectTree(const QnResourceAccessSubject& subject)
{
    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
        return;

    ensureSubjectNode(subject);
    for (auto nodetype : allPlaceholders())
    {
        if (placeholderAllowedForSubject(subject, nodetype))
            ensurePlaceholderNode(subject, nodetype);
    }

    for (const auto& resource: qnResPool->getResources())
        ensureResourceNode(subject, resource);
}

void QnResourceTreeModelUserNodes::removeNode(const QnResourceTreeModelNodePtr& node)
{
    /* Node was already removed. */
    if (!m_allNodes.contains(node))
        return;

    /* Remove node from all hashes where node can be the key. */
    m_allNodes.removeOne(node);
    m_recorders.remove(node);

    /* Recursively remove all child nodes. */
    for (auto child : children(node))
        removeNode(child);

    switch (node->type())
    {
        case Qn::ResourceNode:
        {
            NX_ASSERT(node->resource());
            if (node->resource())
                m_users.remove(node->resource()->getId());
            break;
        }
        case Qn::AllCamerasAccessNode:
        case Qn::AllLayoutsAccessNode:
        case Qn::SharedResourcesNode:
        case Qn::SharedLayoutsNode:
        case Qn::RoleUsersNode:
            for (NodeList& nodes : m_placeholders)
                nodes.removeOne(node);
            break;
        case Qn::SharedLayoutNode:
        case Qn::SharedResourceNode:
            for (NodeList& nodes : m_shared)
                nodes.removeOne(node);
            break;
        case Qn::RecorderNode:
            if (m_recorders.contains(node->parent()))
            {
                RecorderHash& hash = m_recorders[node->parent()];
                hash.remove(hash.key(node));
            }
            break;
        case Qn::RoleNode:
            m_roles.remove(node->uuid());
            break;
        default:
            break;
    }

    removeNodeInternal(node);
}

void QnResourceTreeModelUserNodes::removeNodeInternal(const QnResourceTreeModelNodePtr& node)
{
    node->setResource(QnResourcePtr());
    node->setParent(QnResourceTreeModelNodePtr());
}

void QnResourceTreeModelUserNodes::clean()
{
    for (auto node : m_allNodes)
        removeNodeInternal(node);
    m_recorders.clear();
    m_shared.clear();
    m_placeholders.clear();
    m_users.clear();
    m_roles.clear();
}

void QnResourceTreeModelUserNodes::cleanupRecorders()
{
    QList<QnResourceTreeModelNodePtr> nodesToDelete;
    for (auto node : m_allNodes)
    {
        if (node->type() != Qn::RecorderNode)
            continue;

        if (node->children().isEmpty())
            nodesToDelete << node;
    }

    for (auto node : nodesToDelete)
        removeNode(node);
}

void QnResourceTreeModelUserNodes::handleAccessChanged(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource)
{
    if (qnResourceAccessProvider->hasAccess(subject, resource))
    {
        auto node = ensureResourceNode(subject, resource);
        if (node)
            node->update();
    }
    else
    {
        auto id = subject.id();
        if (!m_shared.contains(id))
            return;
        for (auto node : m_shared[id])
        {
            if (node->resource() != resource)
                continue;

            removeNode(node);
            cleanupRecorders();
            break;
        }
    }
}

void QnResourceTreeModelUserNodes::handleGlobalPermissionsChanged(
    const QnResourceAccessSubject& subject)
{
    /* Full rebuild on current user permissions change. */
    if (subject.user() && subject.user() == context()->user())
    {
        rebuild();
        return;
    }

    auto subjectNode = ensureSubjectNode(subject);
    removeNode(subjectNode);

    //TODO: #GDM really we need only handle permissions change that modifies placeholders
    rebuildSubjectTree(subject);
    cleanupRecorders();
}

void QnResourceTreeModelUserNodes::handleUserEnabledChanged(const QnUserResourcePtr& user)
{
    if (user->isEnabled())
        ensureSubjectNode(user);
    else if (m_users.contains(user->getId()))
        removeNode(m_users.take(user->getId()));
}
