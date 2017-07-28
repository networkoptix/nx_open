#include "resource_tree_model_user_nodes.h"

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <nx_ec/data/api_user_role_data.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/resource_tree_model_node.h>
#include <ui/models/resource/tree/resource_tree_model_layout_node.h>
#include <ui/models/resource/tree/resource_tree_model_recorder_node.h>

#include <ui/workbench/workbench_context.h>


QnResourceTreeModelUserNodes::QnResourceTreeModelUserNodes(
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_model(nullptr),
    m_rootNode(),
    m_allNodes(),
    m_roles(),
    m_users(),
    m_placeholders(),
    m_shared(),
    m_recorders()
{
    connect(resourceAccessProvider(), &QnResourceAccessProvider::accessChanged, this,
        &QnResourceTreeModelUserNodes::handleAccessChanged);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnResourceTreeModelUserNodes::handleUserChanged);

    connect(globalPermissionsManager(), &QnGlobalPermissionsManager::globalPermissionsChanged,
        this, &QnResourceTreeModelUserNodes::handleGlobalPermissionsChanged);

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnResourceTreeModelUserNodes::handleResourceAdded);

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            resource->disconnect(this);
            if (auto user = resource.dynamicCast<QnUserResource>())
            {
                removeUserNode(user);
            }
        });

    connect(userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
        [this](const ec2::ApiUserRoleData& role)
        {
            ensureRoleNode(role)->update();
        });

    connect(userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
        [this](const ec2::ApiUserRoleData& role)
        {
            if (m_roles.contains(role.id))
                removeNode(m_roles.take(role.id));
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
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::rootNode() const
{
    return m_rootNode;
}

void QnResourceTreeModelUserNodes::setRootNode(const QnResourceTreeModelNodePtr& node)
{
    m_rootNode = node;
}

void QnResourceTreeModelUserNodes::initialize(QnResourceTreeModel* model,
    const QnResourceTreeModelNodePtr& rootNode)
{
    setModel(model);
    setRootNode(rootNode);
    handleUserChanged(context()->user());
}

void QnResourceTreeModelUserNodes::rebuild()
{
    NX_ASSERT(m_valid);

    for (const auto& role : userRolesManager()->userRoles())
        rebuildSubjectTree(role);

    for (const auto& user : resourcePool()->getResources<QnUserResource>())
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

    /* Do not show user placeholders under roles. */
    if (subject.user() && subject.user()->userRole() == Qn::UserRole::CustomUserRole)
        return false;

    bool isRole = !subject.user();
    bool hasAllMedia = globalPermissionsManager()->hasGlobalPermission(subject,
        Qn::GlobalAccessAllMediaPermission);
    bool isAdmin = globalPermissionsManager()->hasGlobalPermission(subject,
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

    if (layout->isServiceLayout())
        return false;

    if (layout->isShared())
    {
        /* Admins see all shared layouts. */
        if (globalPermissionsManager()->hasGlobalPermission(subject, Qn::GlobalAdminPermission))
        {
            NX_ASSERT(subject.user());
            if (subject.user())
                return false;
        }

        /* Shared layouts are displayed under role node. */
        if (subject.user() && subject.user()->userRole() == Qn::UserRole::CustomUserRole)
            return false;

        return resourceAccessProvider()->hasAccess(subject, layout);
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
    if (globalPermissionsManager()->hasGlobalPermission(subject,
        Qn::GlobalAccessAllMediaPermission))
            return false;

    /* Shared resources are displayed under role node. */
    if (subject.user() && subject.user()->userRole() == Qn::UserRole::CustomUserRole)
        return false;

    return resourceAccessProvider()->hasAccess(subject, media);
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
    const ec2::ApiUserRoleData& role)
{
    auto pos = m_roles.find(role.id);
    if (pos == m_roles.end())
    {
        NX_ASSERT(!role.isNull());

        QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(m_model, role.id,
            Qn::RoleNode));
        node->initialize();
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
        node->initialize();
        auto parent = m_rootNode;
        if (user->userRole() == Qn::UserRole::CustomUserRole)
        {
            auto role = userRolesManager()->userRole(user->userRoleId());
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
    if (QnResourceAccessFilter::isShareableLayout(resource))
    {
        auto layout = resource.dynamicCast<QnLayoutResource>();
        NX_ASSERT(layout);
        if (layout && showLayoutForSubject(subject, layout))
            return ensureLayoutNode(subject, layout);
    }
    else if (QnResourceAccessFilter::isShareableMedia(resource))
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
    node->initialize();
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
    node->initialize();
    auto parent = ensureSubjectNode(subject);
    if (placeholderAllowedForSubject(subject, Qn::SharedResourcesNode))
        parent = ensurePlaceholderNode(subject, Qn::SharedResourcesNode);

    /* Create recorder nodes for cameras. */
    if (auto camera = media.dynamicCast<QnVirtualCameraResource>())
    {
        QString groupId = camera->getGroupId();
        if (!groupId.isEmpty())
            parent = ensureRecorderNode(parent, camera);
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
    node->initialize();
    node->setParent(ensureSubjectNode(subject));
    node->update();

    placeholders.push_back(node);
    m_allNodes.append(node);
    return node;
}

QnResourceTreeModelNodePtr QnResourceTreeModelUserNodes::ensureRecorderNode(
    const QnResourceTreeModelNodePtr& parentNode,
    const QnVirtualCameraResourcePtr& camera)
{
    auto id = camera->getGroupId();

    auto& recorders = m_recorders[parentNode];
    auto pos = recorders.find(id);
    if (pos == recorders.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelRecorderNode(m_model, camera));
        node->initialize();
        node->setParent(parentNode);

        pos = recorders.insert(id, node);
        m_allNodes.append(*pos);
    }
    return *pos;
}

void QnResourceTreeModelUserNodes::rebuildSubjectTree(const QnResourceAccessSubject& subject)
{
    if (!m_valid)
        return;

    if (auto user = subject.user())
    {
        bool skipUser = !user->isEnabled()
            || (m_model->scope() != QnResourceTreeModel::FullScope
                && user->userRole() == Qn::UserRole::CustomUserRole);

        if (skipUser)
        {
            removeUserNode(subject.user());
            return;
        }
    }

    ensureSubjectNode(subject);
    for (auto nodetype : allPlaceholders())
    {
        if (placeholderAllowedForSubject(subject, nodetype))
            ensurePlaceholderNode(subject, nodetype);
    }

    for (const auto& resource: resourcePool()->getResources())
        ensureResourceNode(subject, resource);
}

void QnResourceTreeModelUserNodes::removeUserNode(const QnUserResourcePtr& user)
{
    auto id = user->getId();
    if (m_users.contains(id))
        removeNode(m_users.take(id));
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

    node->deinitialize();
}

void QnResourceTreeModelUserNodes::clean()
{
    for (auto node: m_allNodes)
        node->deinitialize();
    m_recorders.clear();
    m_shared.clear();
    m_placeholders.clear();
    m_users.clear();
    m_roles.clear();
    m_allNodes.clear();
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

void QnResourceTreeModelUserNodes::handleResourceAdded(const QnResourcePtr& resource)
{
    // Resource was added and instantly removed, data will be processed in handleResourceRemoved
    if (!resource->resourcePool())
        return;

    if (auto user = resource.dynamicCast<QnUserResource>())
    {
        connect(user, &QnUserResource::enabledChanged, this,
            &QnResourceTreeModelUserNodes::rebuildSubjectTree);

        connect(user, &QnUserResource::userRoleChanged, this,
            [this](const QnUserResourcePtr& user)
            {
                removeUserNode(user);
                rebuildSubjectTree(user);
            });

        rebuildSubjectTree(user);
    }
    else if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        /* Here we must handle if common user's layout become shared. */
        auto owner = layout->getParentResource().dynamicCast<QnUserResource>();
        if (!owner)
            return;

        connect(layout, &QnResource::parentIdChanged, this,
            [this, layout, owner]
            {
                handleAccessChanged(owner, layout);
            });
    }
}

void QnResourceTreeModelUserNodes::handleUserChanged(const QnUserResourcePtr& user)
{
    m_valid = !user.isNull()
        && globalPermissionsManager()->hasGlobalPermission(user, Qn::GlobalAdminPermission);

    clean();
    if (m_valid)
        rebuild();
}

void QnResourceTreeModelUserNodes::handleAccessChanged(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource)
{
    if (!m_valid)
        return;

    if (resourceAccessProvider()->hasAccess(subject, resource))
    {
        if (auto node = ensureResourceNode(subject, resource))
        {
            node->update();
            return;
        }
    }

    /* In some cases we should remove nodes under user even if he has access. For example, when
     * user's own layout become shared. */

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

void QnResourceTreeModelUserNodes::handleGlobalPermissionsChanged(
    const QnResourceAccessSubject& subject)
{
    if (!m_valid)
        return;

    if (subject.user() == context()->user())
    {
        /* Rebuild will occur on context user change. */
        return;
    }

    // We got globalPermissionsChanged on user removing.
    if (subject.user() && !subject.user()->resourcePool())
        return;

    auto subjectNode = ensureSubjectNode(subject);
    removeNode(subjectNode);

    // TODO: #GDM really we need only handle permissions change that modifies placeholders
    rebuildSubjectTree(subject);
    cleanupRecorders();
}
