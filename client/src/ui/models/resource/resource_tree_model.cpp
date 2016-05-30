#include "resource_tree_model.h"

#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>

#include <common/common_module.h>

#include <client/client_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_matrix.h>

#include <api/global_settings.h>

#include <ui/actions/action_manager.h>
#include <ui/delegates/resource_tree_model_custom_column_delegate.h>
#include <ui/models/resource/resource_tree_model_node.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_resource.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_access_controller.h>

namespace
{
    const char *pureTreeResourcesOnlyMimeType = "application/x-noptix-pure-tree-resources-only";

    bool intersects(const QStringList &l, const QStringList &r)
    {
        for (const QString &s : l)
            if (r.contains(s))
                return true;
        return false;
    }

    /** Set of top-level node types */
    QList<Qn::NodeType> rootNodeTypes()
    {
        static QList<Qn::NodeType> result;
        if (result.isEmpty())
        {
            result
                << Qn::CurrentSystemNode
                << Qn::SeparatorNode
                << Qn::UsersNode
                << Qn::ServersNode
                << Qn::UserDevicesNode
                << Qn::LayoutsNode
                << Qn::WebPagesNode
                << Qn::LocalResourcesNode
                << Qn::LocalSeparatorNode
                << Qn::OtherSystemsNode
                << Qn::RootNode
                << Qn::BastardNode;
        }
        return result;
    }

} // namespace

// -------------------------------------------------------------------------- //
// QnResourceTreeModel :: contructors, destructor and helpers.
// -------------------------------------------------------------------------- //
QnResourceTreeModel::QnResourceTreeModel(Scope scope, QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_scope(scope)
{
    /* Create top-level nodes. */
    for (Qn::NodeType t : rootNodeTypes())
    {
        m_rootNodes[t] = QnResourceTreeModelNodePtr(new QnResourceTreeModelNode(this, t));
        m_allNodes.append(m_rootNodes[t]);
    }

    /* Connect to context. */
    connect(qnResPool,          &QnResourcePool::resourceAdded,                     this,   &QnResourceTreeModel::at_resPool_resourceAdded,         Qt::QueuedConnection);
    connect(qnResPool,          &QnResourcePool::resourceRemoved,                   this,   &QnResourceTreeModel::at_resPool_resourceRemoved,       Qt::QueuedConnection);
    connect(snapshotManager(),  &QnWorkbenchLayoutSnapshotManager::flagsChanged,    this,   &QnResourceTreeModel::at_snapshotManager_flagsChanged);
    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged,   this,   &QnResourceTreeModel::at_accessController_permissionsChanged);
    connect(context(),          &QnWorkbenchContext::userChanged,                   this,   &QnResourceTreeModel::rebuildTree,                      Qt::QueuedConnection);
    connect(qnCommon,           &QnCommonModule::systemNameChanged,                 this,   &QnResourceTreeModel::at_commonModule_systemNameChanged);
    connect(qnCommon,           &QnCommonModule::readOnlyChanged,                   this,   &QnResourceTreeModel::rebuildTree,                      Qt::QueuedConnection);
    connect(qnGlobalSettings,   &QnGlobalSettings::serverAutoDiscoveryChanged,      this,   &QnResourceTreeModel::at_serverAutoDiscoveryEnabledChanged);
    connect(qnSettings->notifier(QnClientSettings::EXT_INFO_IN_TREE), &QnPropertyNotifier::valueChanged, this, [this](int value)
    {
        Q_UNUSED(value);
        m_rootNodes[rootNodeTypeForScope()]->updateRecursive();
    });
    connect(qnResourceAccessManager, &QnResourceAccessManager::accessibleResourcesChanged, this, &QnResourceTreeModel::at_accessibleResourcesChanged);

    rebuildTree();

    /* It is important to connect before iterating as new resources may be added to the pool asynchronously. */
    for (const QnResourcePtr &resource: qnResPool->getResources())
        at_resPool_resourceAdded(resource);

}

QnResourceTreeModel::~QnResourceTreeModel()
{}

QnResourcePtr QnResourceTreeModel::resource(const QModelIndex &index) const
{
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

QnResourceTreeModelNodePtr QnResourceTreeModel::node(const QModelIndex &index) const
{
    /* Root node. */
    if (!index.isValid())
        return m_rootNodes[rootNodeTypeForScope()];

    return static_cast<QnResourceTreeModelNode *>(index.internalPointer())->toSharedPointer();
}

QnResourceTreeModelNodePtr QnResourceTreeModel::ensureResourceNode(const QnResourcePtr &resource)
{
    if (!resource)
        return m_rootNodes[Qn::BastardNode];

    auto pos = m_resourceNodeByResource.find(resource);
    if (pos == m_resourceNodeByResource.end())
    {
        Qn::NodeType nodeType = Qn::ResourceNode;
        if (accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
            if (QnMediaServerResource::isHiddenServer(resource->getParentResource()))
                nodeType = Qn::EdgeNode;

        pos = m_resourceNodeByResource.insert(resource, QnResourceTreeModelNodePtr(new QnResourceTreeModelNode(this, resource, nodeType)));
        m_nodesByResource[resource].push_back(*pos);
        m_allNodes.append(*pos);
    }
    return *pos;
}

QnResourceTreeModelNodePtr QnResourceTreeModel::ensureItemNode(const QnResourceTreeModelNodePtr& parentNode, const QnUuid& uuid, Qn::NodeType nodeType)
{
    ItemHash& items = m_itemNodesByParent[parentNode];

    auto pos = items.find(uuid);
    if (pos == items.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(this, uuid, nodeType));
        node->setParent(parentNode);

        pos = items.insert(uuid, node);
        m_allNodes.append(*pos);
    }
    return *pos;
}

QnResourceTreeModelNodePtr QnResourceTreeModel::ensureRecorderNode(const QnResourceTreeModelNodePtr& parentNode, const QString& groupId, const QString& groupName)
{
    RecorderHash& recorders = m_recorderHashByParent[parentNode];
    auto pos = recorders.find(groupId);
    if (pos == recorders.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(this, Qn::RecorderNode, !groupName.isEmpty() ? groupName : groupId));
        node->setParent(parentNode);

        pos = recorders.insert(groupId, node);
        m_allNodes.append(*pos);
    }
    return *pos;
}

QnResourceTreeModelNodePtr QnResourceTreeModel::ensureSystemNode(const QString &systemName)
{
    auto pos = m_systemNodeBySystemName.find(systemName);
    if (pos == m_systemNodeBySystemName.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(this, Qn::SystemNode, systemName));
        if (m_scope == FullScope)
            node->setParent(m_rootNodes[Qn::OtherSystemsNode]);
        else
            node->setParent(m_rootNodes[Qn::BastardNode]);

        pos = m_systemNodeBySystemName.insert(systemName, node);
        m_allNodes.append(*pos);
    }
    return *pos;
}

QnResourceTreeModelNodePtr QnResourceTreeModel::ensureSharedLayoutNode(const QnResourceTreeModelNodePtr& parentNode, const QnLayoutResourcePtr& sharedLayout)
{
    NX_ASSERT(sharedLayout->isShared());

    ResourceHash& layouts = m_sharedLayoutNodesByParent[parentNode];
    auto pos = layouts.find(sharedLayout);
    if (pos == layouts.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(this, sharedLayout, Qn::SharedLayoutNode));
        node->setParent(parentNode);

        pos = layouts.insert(sharedLayout, node);
        m_nodesByResource[sharedLayout].push_back(*pos);
        m_allNodes.append(*pos);

        for (auto item : sharedLayout->getItems())
        {
            auto itemNode = ensureItemNode(*pos, item.uuid);
            updateNodeResource(itemNode, qnResPool->getResourceByDescriptor(item.resource));
        }
    }
    return *pos;
}

void QnResourceTreeModel::removeNode(const QnResourceTreeModelNodePtr& node)
{
    /* Node was already removed. */
    if (!m_allNodes.contains(node))
        return;

    /* Make sure resources without parent will never be visible. */
    auto bastardNode = m_rootNodes[Qn::BastardNode];

    m_allNodes.removeOne(node);
    m_recorderHashByParent.remove(node);
    m_itemNodesByParent.remove(node);

    /* Calculating children this way because bastard nodes are absent in node's childred() list. */
    QList<QnResourceTreeModelNodePtr> children;
    for (auto existing : m_allNodes)
        if (existing->parent() == node)
            children << existing;

    /* Recursively remove all child nodes. */
    for (auto child : children)
        removeNode(child);

    /* Remove node from all hashes where node can be the key. */
    m_recorderHashByParent.remove(node);
    m_itemNodesByParent.remove(node);
    m_sharedLayoutNodesByParent.remove(node);

    switch (node->type())
    {
    case Qn::ResourceNode:
    case Qn::EdgeNode:
        m_resourceNodeByResource.remove(node->resource());
        break;
    case Qn::VideoWallItemNode:
    case Qn::VideoWallMatrixNode:
    case Qn::LayoutItemNode:
        if (node->parent())
        {
            ItemHash& hash = m_itemNodesByParent[node->parent()];
            hash.remove(hash.key(node));
        }
        break;
    case Qn::SharedLayoutNode:
        if (node->parent())
        {
            ResourceHash& hash = m_sharedLayoutNodesByParent[node->parent()];
            hash.remove(hash.key(node));
        }
        break;
    case Qn::SystemNode:
        m_systemNodeBySystemName.remove(m_systemNodeBySystemName.key(node));
        break;
    case Qn::RecorderNode:
        if (node->parent())
        {
            RecorderHash& hash = m_recorderHashByParent[node->parent()];
            hash.remove(hash.key(node));
        }
        break;
    default:
        break;
    }

    updateNodeResource(node, QnResourcePtr());
    node->setParent(QnResourceTreeModelNodePtr());
}

QnResourceTreeModelNodePtr QnResourceTreeModel::expectedParent(const QnResourceTreeModelNodePtr& node)
{
    /*
    * Here we can narrow nodes visibility, based on current scope. For example, if we do
    * not want to display cameras in 'Select User' dialog, we set parent to Qn::BastardNode.
    * Also here we restructuring tree for admin and common users.
    */

    bool isLoggedIn = !context()->user().isNull();
    bool isAdmin = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);
    auto bastardNode = m_rootNodes[Qn::BastardNode];
    auto rootNode = m_rootNodes[Qn::RootNode];

    switch (node->type())
    {
    case Qn::RootNode:
    case Qn::BastardNode:
        return QnResourceTreeModelNodePtr();

    case Qn::LocalResourcesNode:
        if (m_scope != FullScope)
            return bastardNode;
        return rootNode;

    case Qn::UsersNode:
        if (m_scope == UsersScope)
            return QnResourceTreeModelNodePtr();    /*< Be the root node in this scope. */
        if (m_scope == FullScope && isAdmin)
            return rootNode;
        return bastardNode;

    case Qn::ServersNode:
        if (m_scope == CamerasScope && isAdmin)
            return QnResourceTreeModelNodePtr();    /*< Be the root node in this scope. */
        if (m_scope == FullScope && isAdmin)
            return rootNode;
        return bastardNode;

    case Qn::CurrentSystemNode:
        if (m_scope == FullScope && isLoggedIn)
            return rootNode;
        return bastardNode;

    case Qn::SeparatorNode:
        if (m_scope == FullScope && isLoggedIn)
            return rootNode;
        return bastardNode;

    case Qn::LocalSeparatorNode:
        if (m_scope == FullScope && !isLoggedIn)
            return rootNode;
        return bastardNode;

    case Qn::LayoutsNode:
        if (m_scope == FullScope && isLoggedIn)
            return rootNode;
        return bastardNode;

    case Qn::OtherSystemsNode:
        if (m_scope == FullScope && isAdmin)
            return rootNode;
        return bastardNode;

    case Qn::UserDevicesNode:
        if (m_scope == CamerasScope && !isAdmin)
            return QnResourceTreeModelNodePtr(); /*< Be the root node in this scope. */
        if (m_scope == FullScope && isLoggedIn && !isAdmin)
            return rootNode;
        return bastardNode;

    case Qn::WebPagesNode:
        if (m_scope == FullScope && isLoggedIn)
            return rootNode;
        return bastardNode;

    case Qn::EdgeNode:
        /* Only admins can see edge nodes. */
        if (m_scope != UsersScope && isAdmin)
            return m_rootNodes[Qn::ServersNode];
        return bastardNode;

    case Qn::ResourceNode:
        return expectedParentForResourceNode(node);

    default:
        break;
    }

    /* LayoutItem nodes and SharedLayout nodes should not change their parents. */
    NX_ASSERT(false, "Should never get here.");
    return bastardNode;
}

QnResourceTreeModelNodePtr QnResourceTreeModel::expectedParentForResourceNode(const QnResourceTreeModelNodePtr& node)
{
    bool isLoggedIn = !context()->user().isNull();
    auto rootNode = m_rootNodes[Qn::RootNode];
    auto bastardNode = m_rootNodes[Qn::BastardNode];
    bool isAdmin = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);

    if (!node->resource())
        return bastardNode;

    if (node->resourceFlags().testFlag(Qn::user))
    {
        if (m_scope == UsersScope)
            return m_rootNodes[Qn::UsersNode];

        if (m_scope == CamerasScope)
            return bastardNode;

        if (node->resource() == context()->user())
            return rootNode;

        if (isAdmin)
            return m_rootNodes[Qn::UsersNode];

        //TODO: #GDM #access remove comment when access rights check will be implemented on server
        //NX_ASSERT(false, "Non-admin user can't see other users.");
        return bastardNode;
    }

    /* In UsersScope all other nodes should be hidden. */
    if (m_scope == UsersScope)
        return bastardNode;

    if (node->resourceFlags().testFlag(Qn::server))
    {
        /* Valid servers. */
        if (!QnMediaServerResource::isFakeServer(node->resource()))
        {
            if (isAdmin)
                return m_rootNodes[Qn::ServersNode];

            return bastardNode;
        }

        /* Fake servers from other systems. */
        if (m_scope == CamerasScope)
            return bastardNode;

        QnMediaServerResourcePtr server = node->resource().staticCast<QnMediaServerResource>();
        QString systemName = server->getSystemName();
        if (systemName == qnCommon->localSystemName())
            return m_rootNodes[Qn::ServersNode];

        return ensureSystemNode(systemName);

    }

    if (node->resourceFlags().testFlag(Qn::videowall))
    {
        if (m_scope != FullScope)
            return bastardNode;
        return m_rootNodes[Qn::RootNode];
    }

    if (node->resourceFlags().testFlag(Qn::web_page))
    {
        if (m_scope != FullScope)
            return bastardNode;
        return m_rootNodes[Qn::WebPagesNode];
    }

    if (QnLayoutResourcePtr layout = node->resource().dynamicCast<QnLayoutResource>())
    {
        if (layout->isFile())
            return m_rootNodes[Qn::LocalResourcesNode];

        if (layout->isShared())
            return m_rootNodes[Qn::LayoutsNode];

        QnResourcePtr owner = layout->getParentResource();
        if (!owner || owner == context()->user())
            return m_rootNodes[Qn::LayoutsNode];

        if (isAdmin)
            return ensureResourceNode(owner);

        return bastardNode;
    }

    /* Storages are not to be displayed to users. */
    if (node->resource().dynamicCast<QnStorageResource>())
        return bastardNode;

    /* Checking local resources. */
    auto parentResource = node->resource()->getParentResource();
    if (!parentResource || parentResource->flags().testFlag(Qn::local_server))
    {
        if (node->resourceFlags().testFlag(Qn::local))
        {
            if (isLoggedIn)
                return m_rootNodes[Qn::LocalResourcesNode];
            return rootNode;
        }
        return bastardNode;
    }

    /* Checking cameras in the exported nov-files. */
    if (parentResource->flags().testFlag(Qn::local_layout))
    {
        if (node->resourceFlags().testFlag(Qn::local))
            return ensureResourceNode(parentResource);
        return bastardNode;
    }

    if (QnVirtualCameraResourcePtr camera = node->resource().dynamicCast<QnVirtualCameraResource>())
    {
        auto parentNode = isAdmin
            ? ensureResourceNode(parentResource)
            : m_rootNodes[Qn::UserDevicesNode];

        QString groupName = camera->getGroupName();
        QString groupId = camera->getGroupId();
        if (!groupId.isEmpty())
            return ensureRecorderNode(parentNode, groupId, groupName);

        return parentNode;
    }

    NX_ASSERT(false, "Should never get here.");
    return bastardNode;
}

void QnResourceTreeModel::updateNodeParent(const QnResourceTreeModelNodePtr& node)
{
    node->setParent(expectedParent(node));
}

void QnResourceTreeModel::updateNodeResource(const QnResourceTreeModelNodePtr& node, const QnResourcePtr& resource)
{
    if (node->resource() == resource)
        return;

    if (node->resource())
        m_nodesByResource[node->resource()].removeAll(node);
    node->setResource(resource);
    if (resource)
        m_nodesByResource[resource].push_back(node);
}

void QnResourceTreeModel::updateSharedLayoutNodes(const QnLayoutResourcePtr& layout)
{
    QnUserResourceList usersToUpdate;

    /* Here we should add or update shared layout nodes in all existing in the tree users. */
    for (auto iter = m_resourceNodeByResource.cbegin(); iter != m_resourceNodeByResource.cend(); ++iter)
    {
        QnResourcePtr resource = iter.key();
        if (!resource->flags().testFlag(Qn::user))
            continue;

        QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
        NX_ASSERT(user);
        if (!user)
            continue;

        /* User is already removed, we still haven't deleted node from the nodes list. */
        if (!user->resourcePool())
            continue;

        auto parentNode = *iter;

        /* Copy of the list before all changes. */
        auto sharedLayoutNodes = m_sharedLayoutNodesByParent[parentNode].values();

        /* No shared layouts nodes must exist under current user node. */
        if (user == context()->user())
        {
            usersToUpdate << user;
            continue;
        }

        /* Add node if required. */
        if (qnResourceAccessManager->hasPermission(user, layout, Qn::ReadPermission))
        {
            usersToUpdate << user;
            continue;
        }

        /* Check if we need to remove node */
        for (auto childNode : sharedLayoutNodes)
        {
            if (childNode->resource() != layout)
                continue;

            usersToUpdate << user;
            break;
        }
    }

    for (const auto& user : usersToUpdate)
        updateSharedLayoutNodesForUser(user);
}

void QnResourceTreeModel::updateSharedLayoutNodesForUser(const QnUserResourcePtr& user)
{
    auto iter = m_resourceNodeByResource.find(user);
    if (iter == m_resourceNodeByResource.end())
        return;

    QList<QnResourceTreeModelNodePtr> nodesToDelete;
    auto parentNode = *iter;

    /* Copy of the list before all changes. */
    auto sharedLayoutNodes = m_sharedLayoutNodesByParent[parentNode].values();

    /* No shared layouts nodes must exist under current user node. */
    if (user == context()->user())
    {
        nodesToDelete << sharedLayoutNodes;
    }
    else
    {
        auto accessibleResources = qnResourceAccessManager->accessibleResources(user->getId());

        /* Add new shared layouts and forcibly update them. */
        for (const QnUuid& id : accessibleResources)
        {
            QnLayoutResourcePtr layout = qnResPool->getResourceById<QnLayoutResource>(id);
            if (!layout || !layout->isShared())
                continue;

            ensureSharedLayoutNode(parentNode, layout)->update();
        }

        /* Cleanup nodes that are not available anymore. */
        for (auto existingNode : sharedLayoutNodes)
        {
            NX_ASSERT(existingNode && existingNode->resource());
            if (!existingNode || !existingNode->resource())
                continue;

            if (!accessibleResources.contains(existingNode->resource()->getId()))
                nodesToDelete << existingNode;
        }
    }

    for (auto node : nodesToDelete)
        removeNode(node);
}

Qn::NodeType QnResourceTreeModel::rootNodeTypeForScope() const
{
    switch (m_scope)
    {
    case QnResourceTreeModel::CamerasScope:
        return accessController()->hasGlobalPermission(Qn::GlobalAdminPermission)
            ? Qn::ServersNode
            : Qn::UserDevicesNode;
    case QnResourceTreeModel::UsersScope:
        return Qn::UsersNode;
    default:
        return Qn::RootNode;
    }
}

void QnResourceTreeModel::cleanupSystemNodes()
{
    QList<QnResourceTreeModelNodePtr> nodesToDelete;
    for (auto node : m_allNodes)
    {
        if (node->type() != Qn::SystemNode)
            continue;

        if (node->children().isEmpty())
            nodesToDelete << node;
    }

    for (auto node : nodesToDelete)
        removeNode(node);
}

QnResourceTreeModelCustomColumnDelegate* QnResourceTreeModel::customColumnDelegate() const {
    return m_customColumnDelegate.data();
}

void QnResourceTreeModel::setCustomColumnDelegate(QnResourceTreeModelCustomColumnDelegate *columnDelegate) {
    if (m_customColumnDelegate == columnDelegate)
        return;

    if (m_customColumnDelegate)
        disconnect(m_customColumnDelegate, nullptr, this, nullptr);

    m_customColumnDelegate = columnDelegate;

    auto notifyCustomColumnChanged = [this]()
    {
        //TODO: #GDM update only custom column and changed rows
        Qn::NodeType rootNodeType = rootNodeTypeForScope();
        m_rootNodes[rootNodeType]->updateRecursive();
    };

    if (m_customColumnDelegate)
        connect(m_customColumnDelegate, &QnResourceTreeModelCustomColumnDelegate::notifyDataChanged,
                this, notifyCustomColumnChanged);

    notifyCustomColumnChanged();

}


// -------------------------------------------------------------------------- //
// QnResourceTreeModel :: QAbstractItemModel implementation
// -------------------------------------------------------------------------- //
QModelIndex QnResourceTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if(!hasIndex(row, column, parent)) /* hasIndex calls rowCount and columnCount. */
        return QModelIndex();

    return node(parent)->child(row)->createIndex(row, column);
}

QModelIndex QnResourceTreeModel::buddy(const QModelIndex &index) const
{
    return index;
}

QModelIndex QnResourceTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid() || index.model() != this)
        return QModelIndex();
    return node(index)->parent()->createIndex(Qn::NameColumn);
}

bool QnResourceTreeModel::hasChildren(const QModelIndex &parent) const
{
    return rowCount(parent) > 0;
}

int QnResourceTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > Qn::NameColumn)
        return 0;

    return node(parent)->children().size();
}

int QnResourceTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return Qn::ColumnCount;
}

Qt::ItemFlags QnResourceTreeModel::flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return Qt::NoItemFlags;

    if (index.column() == Qn::CustomColumn)
        return m_customColumnDelegate ? m_customColumnDelegate->flags(index) : Qt::NoItemFlags;

    return node(index)->flags(index.column());
}

QVariant QnResourceTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    /* Only standard QT roles are subject to reimplement. Otherwise we may go to recursion, getting resource for example. */
    if (index.column() == Qn::CustomColumn && role <= Qt::UserRole)
        return m_customColumnDelegate ? m_customColumnDelegate->data(index, role) : QVariant();

    return node(index)->data(role, index.column());
}

bool QnResourceTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid())
        return false;

    if (index.column() == Qn::CustomColumn)
        return m_customColumnDelegate ? m_customColumnDelegate->setData(index, value, role) : false;

    return node(index)->setData(value, role, index.column());
}

QVariant QnResourceTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section);
    Q_UNUSED(orientation);
    Q_UNUSED(role);

    return QVariant(); /* No headers needed. */
}

QHash<int,QByteArray> QnResourceTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles = base_type::roleNames();
    roles.insert(Qn::ResourceRole,              "resource");
    roles.insert(Qn::ResourceFlagsRole,         "flags");
    roles.insert(Qn::ItemUuidRole,              "uuid");
    roles.insert(Qn::ResourceSearchStringRole,  "searchString");
    roles.insert(Qn::ResourceStatusRole,        "status");
    roles.insert(Qn::NodeTypeRole,              "nodeType");
    return roles;
}

QStringList QnResourceTreeModel::mimeTypes() const
{
    QStringList result = QnWorkbenchResource::resourceMimeTypes();
    result.append(QLatin1String(pureTreeResourcesOnlyMimeType));
    result.append(QnVideoWallItem::mimeType());
    return result;
}

QMimeData *QnResourceTreeModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = base_type::mimeData(indexes);
    if (!mimeData)
        return nullptr;

    const QStringList types = mimeTypes();

    /*
     * This flag services the assertion that cameras can be dropped on server
     * if and only if they are dragged from tree (not from layouts)
     */
    bool pureTreeResourcesOnly = true;

    if (intersects(types, QnWorkbenchResource::resourceMimeTypes()))
    {
        QnResourceList resources;
        for (const QModelIndex &index : indexes)
        {
            auto node = this->node(index);

            if (node && node->type() == Qn::RecorderNode)
            {
                for (auto child : node->children())
                {
                    if (child->resource() && !resources.contains(child->resource()))
                        resources.append(child->resource());
                }
            }

            if (node && node->resource() && !resources.contains(node->resource()))
                resources.append(node->resource());

            if (node && (node->type() == Qn::LayoutItemNode))
                pureTreeResourcesOnly = false;
        }

        QnWorkbenchResource::serializeResources(resources, types, mimeData);
    }

    if (types.contains(QnVideoWallItem::mimeType()))
    {
        QSet<QnUuid> uuids;
        for(const QModelIndex &index: indexes)
        {
            auto node = this->node(index);

            if (node && (node->type() == Qn::VideoWallItemNode))
                uuids << node->uuid();
        }
        QnVideoWallItem::serializeUuids(uuids.toList(), mimeData);

    }

    if (types.contains(QLatin1String(pureTreeResourcesOnlyMimeType)))
        mimeData->setData(QLatin1String(pureTreeResourcesOnlyMimeType), QByteArray(pureTreeResourcesOnly ? "1" : "0"));

    return mimeData;
}

bool QnResourceTreeModel::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (!mimeData)
        return false;

    /* Check if the action is supported. */
    if (!supportedDropActions().testFlag(action))
        return false;

    /* Check if the format is supported. */
    if (!intersects(mimeData->formats(), QnWorkbenchResource::resourceMimeTypes()) && !mimeData->formats().contains(QnVideoWallItem::mimeType()))
        return base_type::dropMimeData(mimeData, action, row, column, parent);

    /* Decode. */
    QnResourceList sourceResources = QnWorkbenchResource::deserializeResources(mimeData);

    /* Check where we're dropping it. */
    auto node = this->node(parent);

    if (node->type() == Qn::LayoutItemNode)
        node = node->parent(); /* Dropping into an item is the same as dropping into a layout. */

    if (node->parent() && (node->parent()->resourceFlags().testFlag(Qn::server)))
        node = node->parent(); /* Dropping into a server item is the same as dropping into a server */


    /* Drop on videowall is handled in videowall. */
    if (node->type() == Qn::VideoWallItemNode)
    {
        QnActionParameters parameters;
        if (mimeData->hasFormat(QnVideoWallItem::mimeType()))
            parameters = QnActionParameters(qnResPool->getVideoWallItemsByUuid(QnVideoWallItem::deserializeUuids(mimeData)));
        else
            parameters = QnActionParameters(sourceResources);
        parameters.setArgument(Qn::VideoWallItemGuidRole, node->uuid());
        menu()->trigger(QnActions::DropOnVideoWallItemAction, parameters);
    }
    else
    {
        handleDrop(sourceResources, node->resource(), mimeData);
    }

    return true;
}

Qt::DropActions QnResourceTreeModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}


// -------------------------------------------------------------------------- //
// QnResourceTreeModel :: handlers
// -------------------------------------------------------------------------- //
void QnResourceTreeModel::at_resPool_resourceAdded(const QnResourcePtr &resource)
{
    NX_ASSERT(resource);
    if (!resource)
        return;

    connect(resource,       &QnResource::parentIdChanged,                this,  &QnResourceTreeModel::at_resource_parentIdChanged);
    connect(resource,       &QnResource::nameChanged,                    this,  &QnResourceTreeModel::at_resource_resourceChanged);
    connect(resource,       &QnResource::statusChanged,                  this,  &QnResourceTreeModel::at_resource_resourceChanged);
    connect(resource,       &QnResource::urlChanged,                     this,  &QnResourceTreeModel::at_resource_resourceChanged);
    connect(resource,       &QnResource::flagsChanged,                   this,  &QnResourceTreeModel::at_resource_resourceChanged);

    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    if (layout)
    {
        connect(layout,     &QnLayoutResource::itemAdded,               this,   &QnResourceTreeModel::at_layout_itemAdded);
        connect(layout,     &QnLayoutResource::itemRemoved,             this,   &QnResourceTreeModel::at_layout_itemRemoved);
    }

    QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
    if (videoWall)
    {
        connect(videoWall,  &QnVideoWallResource::itemAdded,            this,   &QnResourceTreeModel::at_videoWall_itemAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::itemChanged,          this,   &QnResourceTreeModel::at_videoWall_itemAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::itemRemoved,          this,   &QnResourceTreeModel::at_videoWall_itemRemoved);

        connect(videoWall,  &QnVideoWallResource::matrixAdded,          this,   &QnResourceTreeModel::at_videoWall_matrixAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::matrixChanged,        this,   &QnResourceTreeModel::at_videoWall_matrixAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::matrixRemoved,        this,   &QnResourceTreeModel::at_videoWall_matrixRemoved);
    }

    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (camera)
    {
        connect(camera,     &QnVirtualCameraResource::groupIdChanged,   this,   &QnResourceTreeModel::at_resource_parentIdChanged);
        connect(camera,     &QnVirtualCameraResource::groupNameChanged, this,   &QnResourceTreeModel::at_camera_groupNameChanged);
        connect(camera,     &QnVirtualCameraResource::statusFlagsChanged, this, &QnResourceTreeModel::at_resource_resourceChanged);
        auto updateParent = [this](const QnResourcePtr &resource)
        {
            /* Automatically update display name of the EDGE server if its camera was renamed. */
            QnResourcePtr parent = resource->getParentResource();
            if (QnMediaServerResource::isEdgeServer(parent))
                at_resource_resourceChanged(parent);
        };
        connect(camera, &QnResource::nameChanged, this, updateParent);
        updateParent(camera);
    }

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (server)
    {
        connect(server,     &QnMediaServerResource::redundancyChanged,  this,   &QnResourceTreeModel::at_server_redundancyChanged);

        if (QnMediaServerResource::isFakeServer(server))
            connect(server, &QnMediaServerResource::systemNameChanged,  this,   &QnResourceTreeModel::at_server_systemNameChanged);
    }

    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (user)
    {
        connect(user, &QnUserResource::enabledChanged, this, &QnResourceTreeModel::at_user_enabledChanged);
    }

    auto node = ensureResourceNode(resource);
    updateNodeParent(node);

    if (server)
    {
        for (const QnResourcePtr &camera : qnResPool->getResourcesByParentId(server->getId()))
        {
            if (m_resourceNodeByResource.contains(camera))
                at_resource_parentIdChanged(camera);
        }
    }

    if (layout)
    {
        for (const QnLayoutItemData &item : layout->getItems())
            at_layout_itemAdded(layout, item);
        if (layout->isShared())
            updateSharedLayoutNodes(layout);
    }

    if (videoWall)
    {
        for (const QnVideoWallItem &item : videoWall->items()->getItems())
            at_videoWall_itemAddedOrChanged(videoWall, item);
        for (const QnVideoWallMatrix &matrix : videoWall->matrices()->getItems())
            at_videoWall_matrixAddedOrChanged(videoWall, matrix);
    }

    if (user)
    {
          updateSharedLayoutNodesForUser(user);
    }
}

void QnResourceTreeModel::at_resPool_resourceRemoved(const QnResourcePtr &resource)
{
    if (!resource)
        return;

    disconnect(resource, NULL, this, NULL);

    QList<QnResourceTreeModelNodePtr> nodesToDelete;
    for (auto node: m_allNodes)
    {
        if (node->resource() != resource)
            continue;

        if (node->type() == Qn::VideoWallItemNode)
            updateNodeResource(node, QnResourcePtr());
        else
            nodesToDelete << node;
    }

    for (auto node: nodesToDelete)
        removeNode(node);

    m_nodesByResource.remove(resource);
    m_resourceNodeByResource.remove(resource);

    if (QnMediaServerResource::isFakeServer(resource))
        cleanupSystemNodes();
}


void QnResourceTreeModel::rebuildTree()
{
    for (auto nodeType : rootNodeTypes())
    {
        auto node = m_rootNodes[nodeType];
        updateNodeParent(node);
        node->update();
    }

    for (auto node : m_resourceNodeByResource)
        updateNodeParent(node);
}

void QnResourceTreeModel::handleDrop(const QnResourceList& sourceResources, const QnResourcePtr& targetResource, const QMimeData *mimeData)
{
    if (sourceResources.isEmpty() || !targetResource)
        return;

    /* We can add media resources to layout */
    if (QnLayoutResourcePtr layout = targetResource.dynamicCast<QnLayoutResource>())
    {
        QnResourceList medias;
        for (const QnResourcePtr& res : sourceResources)
        {
            if (res.dynamicCast<QnMediaResource>())
                medias.push_back(res);
        }
        if (!medias.isEmpty())
            menu()->trigger(
                QnActions::OpenInLayoutAction,
                QnActionParameters(medias).
                withArgument(Qn::LayoutResourceRole, layout)
            );
    }

    /* Drop layout on user means sharing this layout. */
    else if (QnUserResourcePtr targetUser = targetResource.dynamicCast<QnUserResource>())
    {
        for (const QnLayoutResourcePtr &sourceLayout : sourceResources.filtered<QnLayoutResource>())
        {
            if (sourceLayout->getParentId() == targetUser->getId())
                continue; /* Dropping resource into its owner does nothing. */

            if (sourceLayout->isFile())
                continue;

            menu()->trigger(
                QnActions::ShareLayoutAction,
                QnActionParameters(sourceLayout).
                withArgument(Qn::UserResourceRole, targetUser)
            );
        }
    }

    /* Drop camera on server allows to move servers between cameras. */
    else if (QnMediaServerResourcePtr server = targetResource.dynamicCast<QnMediaServerResource>())
    {
        if (QnMediaServerResource::isFakeServer(server))
            return;

        /* Do not allow to drop camera items from layouts. */
        if (mimeData->data(QLatin1String(pureTreeResourcesOnlyMimeType)) != QByteArray("1"))
            return;

        QnVirtualCameraResourceList cameras = sourceResources.filtered<QnVirtualCameraResource>();
        if (!cameras.empty())
            menu()->trigger(
                QnActions::MoveCameraAction,
                QnActionParameters(cameras).
                withArgument(Qn::MediaServerResourceRole, server)
            );
    }
}

void QnResourceTreeModel::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &layout)
{
    QnVideoWallResourcePtr videowall = layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    auto node = videowall
        ? ensureResourceNode(videowall)
        : ensureResourceNode(layout);

    node->setModified(snapshotManager()->isModified(layout));
    node->update();
}

void QnResourceTreeModel::at_accessController_permissionsChanged(const QnResourcePtr &resource)
{
    if (resource == context()->user())
    {
        rebuildTree();
        return;
    }

    for (auto node : m_nodesByResource[resource])
        node->update();

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (layout->isShared())
            updateSharedLayoutNodes(layout);
    }
}

void QnResourceTreeModel::at_resource_parentIdChanged(const QnResourcePtr &resource)
{
    auto node = ensureResourceNode(resource);

    /* Update edge resource if we are the admin. */
    if (accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
    {
        if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        {
            auto server = camera->getParentServer();
            bool wasEdge = (node->type() == Qn::EdgeNode);
            bool mustBeEdge = QnMediaServerResource::isHiddenServer(server);
            if (wasEdge != mustBeEdge)
            {
                auto serverNode = node->parent();

                m_resourceNodeByResource.remove(camera);
                removeNode(node);

                /* Re-create node with changed type. */
                node = ensureResourceNode(camera);
                if (serverNode)
                    serverNode->update();
            }
        }
    }

    updateNodeParent(node);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
        updateSharedLayoutNodes(layout);
}

void QnResourceTreeModel::at_resource_resourceChanged(const QnResourcePtr &resource)
{
    if (m_nodesByResource.contains(resource))
        for (auto node: m_nodesByResource[resource])
            node->update();
}

void QnResourceTreeModel::at_layout_itemAdded(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item)
{
    for (auto parentNode : m_nodesByResource[layout])
    {
        if (parentNode->type() != Qn::ResourceNode && parentNode->type() != Qn::SharedLayoutNode)
            continue;

        auto node = ensureItemNode(parentNode, item.uuid);
        QnResourcePtr resource = qnResPool->getResourceByDescriptor(item.resource);
        updateNodeResource(node, resource); /* In case item is just created. */
    }

}

void QnResourceTreeModel::at_layout_itemRemoved(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item)
{
    for (auto parentNode : m_nodesByResource[layout])
    {
        if (parentNode->type() != Qn::ResourceNode && parentNode->type() != Qn::SharedLayoutNode)
            continue;

        auto node = m_itemNodesByParent[parentNode].take(item.uuid);
        if (node)
            removeNode(node);
    }
}

void QnResourceTreeModel::at_videoWall_itemAddedOrChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item)
{
    auto parentNode = ensureResourceNode(videoWall);
    auto node = ensureItemNode(parentNode, item.uuid, Qn::VideoWallItemNode);

    QnResourcePtr resource;
    if (!item.layout.isNull())
        resource = resourcePool()->getResourceById(item.layout);

    if (node->resource() != resource)
        updateNodeResource(node, resource);
    else
        node->update(); // in case of _changed method call, where setResource will exit instantly
}

void QnResourceTreeModel::at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item)
{
    auto parentNode = ensureResourceNode(videoWall);
    auto node = m_itemNodesByParent[parentNode].take(item.uuid);
    if (node)
        removeNode(node);
}

void QnResourceTreeModel::at_videoWall_matrixAddedOrChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallMatrix &matrix)
{
    auto parentNode = ensureResourceNode(videoWall);
    auto node = ensureItemNode(parentNode, matrix.uuid, Qn::VideoWallMatrixNode);
    node->update(); //TODO: #GDM what for?
}

void QnResourceTreeModel::at_videoWall_matrixRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallMatrix &matrix)
{
    auto parentNode = ensureResourceNode(videoWall);
    auto node = m_itemNodesByParent[parentNode].take(matrix.uuid);
    if (node)
        removeNode(node);
}

void QnResourceTreeModel::at_camera_groupNameChanged(const QnResourcePtr &resource)
{
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    NX_ASSERT(camera);
    if (!camera)
        return;

    const QString groupId = camera->getGroupId();
    for (RecorderHash recorderHash: m_recorderHashByParent)
    {
        if (!recorderHash.contains(groupId))
            continue;
        auto recorder = recorderHash[groupId];
        recorder->m_name = camera->getGroupName();
        recorder->m_displayName = camera->getGroupName();
        recorder->changeInternal();
    }
}

void QnResourceTreeModel::at_server_systemNameChanged(const QnResourcePtr &resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    NX_ASSERT(server);
    if (!server)
        return;

    auto node = ensureResourceNode(resource);
    updateNodeParent(node);
    cleanupSystemNodes();
}

void QnResourceTreeModel::at_server_redundancyChanged(const QnResourcePtr &resource)
{
    auto node = ensureResourceNode(resource);
    node->update();

    /* Update edge nodes if we are the admin. */
    if (accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
    {
        for (const QnVirtualCameraResourcePtr &cameraResource : qnResPool->getAllCameras(resource, true))
        {
            auto existingNode = m_resourceNodeByResource.take(cameraResource);
            removeNode(existingNode);

            /* Re-create node as it should change its NodeType from ResourceNode to EdgeNode or vice versa. */
            auto updatedNode = ensureResourceNode(cameraResource);
            updateNodeParent(updatedNode);
        }
    }
}

void QnResourceTreeModel::at_commonModule_systemNameChanged()
{
    m_rootNodes[Qn::CurrentSystemNode]->update();
}

void QnResourceTreeModel::at_user_enabledChanged(const QnResourcePtr &resource)
{
    auto node = ensureResourceNode(resource);
    node->update();
}

void QnResourceTreeModel::at_serverAutoDiscoveryEnabledChanged()
{
    m_rootNodes[Qn::OtherSystemsNode]->update();
}

void QnResourceTreeModel::at_accessibleResourcesChanged(const QnUuid& userId)
{
    auto user = qnResPool->getResourceById<QnUserResource>(userId);
    if (context()->user() == user)
        rebuildTree();
    else
        updateSharedLayoutNodesForUser(user);
}
