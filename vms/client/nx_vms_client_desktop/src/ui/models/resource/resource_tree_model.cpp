#include "resource_tree_model.h"

#include <nx/vms/client/desktop/ini.h>

#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_module.h>

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

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
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <api/global_settings.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/resource_views/data/node_type.h>
#include <nx/vms/client/desktop/utils/wearable_manager.h>
#include <nx/vms/client/desktop/utils/wearable_state.h>
#include <ui/delegates/resource_tree_model_custom_column_delegate.h>

#include <ui/models/resource/resource_tree_model_node.h>
#include <ui/models/resource/resource_tree_model_node_factory.h>
#include <ui/models/resource/tree/resource_tree_model_user_nodes.h>
#include <ui/models/resource/tree/resource_tree_model_recorder_node.h>
#include <ui/models/resource/tree/resource_tree_model_node_manager.h>
#include <ui/models/resource/tree/resource_tree_model_layout_node_manager.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_access_controller.h>

#include <nx/vms/client/desktop/utils/mime_data.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;
using namespace nx::vms::common;

#define DEBUG_RESOURCE_TREE_MODEL
#ifdef DEBUG_RESOURCE_TREE_MODEL
#define TRACE(...) qDebug() << "QnResourceTreeModel: " << __VA_ARGS__;
#else
#define TRACE(...)
#endif

namespace {

static const QString pureTreeResourcesOnlyMimeType = lit("application/x-noptix-pure-tree-resources-only");

bool intersects(const QStringList &l, const QStringList &r)
{
    for (const QString &s : l)
        if (r.contains(s))
            return true;
    return false;
}

/** Set of top-level node types */
QList<ResourceTreeNodeType> rootNodeTypes()
{
    static QList<ResourceTreeNodeType> result;
    if (result.isEmpty())
    {
        result
            << ResourceTreeNodeType::currentSystem
            << ResourceTreeNodeType::currentUser
            << ResourceTreeNodeType::separator
            << ResourceTreeNodeType::users
            << ResourceTreeNodeType::servers
            << ResourceTreeNodeType::filteredServers
            << ResourceTreeNodeType::filteredCameras
            << ResourceTreeNodeType::filteredVideowalls
            << ResourceTreeNodeType::filteredUsers
            << ResourceTreeNodeType::filteredLayouts
            << ResourceTreeNodeType::userResources
            << ResourceTreeNodeType::layouts
            << ResourceTreeNodeType::layoutTours
            << ResourceTreeNodeType::webPages
            << ResourceTreeNodeType::analyticsEngines
            << ResourceTreeNodeType::localResources
            << ResourceTreeNodeType::localSeparator
            << ResourceTreeNodeType::otherSystems
            << ResourceTreeNodeType::root
            << ResourceTreeNodeType::bastard;
    }
    return result;
}

} // anonymous namespace

// -------------------------------------------------------------------------- //
// QnResourceTreeModel :: contructors, destructor and helpers.
// -------------------------------------------------------------------------- //
QnResourceTreeModel::QnResourceTreeModel(
    Scope scope,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_scope(scope),
    m_nodeManager(new QnResourceTreeModelNodeManager(this)),
    m_layoutNodeManager(new QnResourceTreeModelLayoutNodeManager(this))
{
    /* Create top-level nodes. */
    for (NodeType nodeType : rootNodeTypes())
        m_rootNodes[nodeType] = QnResourceTreeModelNodeFactory::createNode(nodeType, this, false);

    for (auto rootNode: m_rootNodes)
    {
        if (rootNode)
        {
            m_allNodes.append(rootNode);
            updateNodeParent(rootNode);
            rootNode->initialize();
        }
    }

    if (scope != CamerasScope)
    {
        auto userNodes = new QnResourceTreeModelUserNodes(this);
        userNodes->initialize(this, m_rootNodes[NodeType::users]);
    }

    /* Connect to context. */
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnResourceTreeModel::at_resPool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnResourceTreeModel::at_resPool_resourceRemoved);
    connect(snapshotManager(), &QnWorkbenchLayoutSnapshotManager::layoutFlagsChanged, this,
        &QnResourceTreeModel::at_snapshotManager_flagsChanged);
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnResourceTreeModel::rebuildTree);
    connect(qnGlobalSettings, &QnGlobalSettings::systemNameChanged, this,
        &QnResourceTreeModel::at_systemNameChanged);

    connect(qnSettings->notifier(QnClientSettings::EXTRA_INFO_IN_TREE),
        &QnPropertyNotifier::valueChanged,
        this,
        [this]
        {
            const auto root = m_rootNodes[rootNodeTypeForScope()];
            NX_ASSERT(root, lit("Absent root for scope %1: type of %2")
                .arg(m_scope).arg((int)rootNodeTypeForScope()));
            if (root)
                root->updateRecursive();
        });

    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged, this,
        &QnResourceTreeModel::handlePermissionsChanged);

    connect(qnClientModule->wearableManager(), &WearableManager::stateChanged, this,
        &QnResourceTreeModel::at_wearableManager_stateChanged);

    rebuildTree();

    /* It is important to connect before iterating as new resources may be added to the pool asynchronously. */
    for (const QnResourcePtr& resource: resourcePool()->getResources())
        at_resPool_resourceAdded(resource);
}

QnResourceTreeModel::~QnResourceTreeModel()
{
    QSignalBlocker scopedSignalBlocker(this); // TODO: #vbreus Ideally this shouldn't be needed
    for (auto node: m_allNodes)
        node->deinitialize();
}

QnResourcePtr QnResourceTreeModel::resource(const QModelIndex& index) const
{
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

QnResourceTreeModelNodePtr QnResourceTreeModel::node(const QModelIndex& index) const
{
    /* Root node. */
    if (!index.isValid())
    {
        const auto root = m_rootNodes[rootNodeTypeForScope()];
        NX_ASSERT(root, lit("Absent root for scope %1: type of %2")
            .arg(m_scope).arg((int)rootNodeTypeForScope()));
        return root;
    }

    return static_cast<QnResourceTreeModelNode *>(index.internalPointer())->toSharedPointer();
}

QList<QnResourceTreeModelNodePtr> QnResourceTreeModel::children(const QnResourceTreeModelNodePtr& node) const
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

QnResourceTreeModelNodePtr QnResourceTreeModel::ensureResourceNode(const QnResourcePtr& resource)
{
    if (!resource)
        return m_rootNodes[NodeType::bastard];

    auto pos = m_resourceNodeByResource.find(resource);
    if (pos == m_resourceNodeByResource.end())
    {
        const auto node = QnResourceTreeModelNodeFactory::createResourceNode(resource, this);

        pos = m_resourceNodeByResource.insert(resource, node);
        m_nodesByResource[resource].push_back(node);
        m_allNodes.append(node);
    }
    return *pos;
}

QnResourceTreeModelNodePtr QnResourceTreeModel::ensureItemNode(
    const QnResourceTreeModelNodePtr& parentNode,
    const QnUuid& uuid,
    NodeType nodeType)
{
    ItemHash& items = m_itemNodesByParent[parentNode];

    auto pos = items.find(uuid);
    if (pos == items.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(this, uuid, nodeType));
        node->initialize();
        node->setParent(parentNode);

        pos = items.insert(uuid, node);
        m_allNodes.append(node);
    }
    return *pos;
}

QnResourceTreeModelNodePtr QnResourceTreeModel::ensureRecorderNode(
    const QnResourceTreeModelNodePtr& parentNode, const QnVirtualCameraResourcePtr& camera)
{
    auto id = camera->getGroupId();

    RecorderHash& recorders = m_recorderHashByParent[parentNode];
    auto pos = recorders.find(id);
    if (pos == recorders.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelRecorderNode(this, camera));
        node->initialize();
        node->setParent(parentNode);

        pos = recorders.insert(id, node);
        m_allNodes.append(node);
    }
    return *pos;
}

void QnResourceTreeModel::removeNode(QnResourceTreeModelNodePtr node)
{
    /* Node was already removed. */
    if (!m_allNodes.contains(node))
        return;

    /* Remove node from all hashes where node can be the key. */
    auto resource = node->resource();
    node->deinitialize();
    m_allNodes.removeOne(node);
    m_recorderHashByParent.remove(node);
    m_itemNodesByParent.remove(node);
    if (resource)
        m_nodesByResource[resource].removeAll(node);

    /* Recursively remove all child nodes. */
    for (auto child: children(node))
        removeNode(child);

    switch (node->type())
    {
    case NodeType::resource:
    case NodeType::edge:
        m_resourceNodeByResource.remove(node->resource());
        break;
    case NodeType::videoWallItem:
    case NodeType::videoWallMatrix:
    case NodeType::layoutItem:
        if (node->parent())
        {
            ItemHash& hash = m_itemNodesByParent[node->parent()];
            hash.remove(hash.key(node));
        }
        break;
    case NodeType::recorder:
        if (node->parent())
        {
            RecorderHash& hash = m_recorderHashByParent[node->parent()];
            hash.remove(hash.key(node));
        }
        break;
    default:
        break;
    }
}

QnResourceTreeModelNodePtr QnResourceTreeModel::expectedParent(const QnResourceTreeModelNodePtr& node)
{
    /*
    * Here we can narrow nodes visibility, based on current scope. For example, if we do
    * not want to display cameras in 'Select User' dialog, we set parent to Qn::BastardNode.
    * Also here we restructuring tree for admin and common users.
    */

    bool isLoggedIn = !context()->user().isNull();
    bool isAdmin = accessController()->hasGlobalPermission(GlobalPermission::admin);
    auto bastardNode = m_rootNodes[NodeType::bastard];
    auto rootNode = m_rootNodes[NodeType::root];

    switch (node->type())
    {
    case NodeType::root:
    case NodeType::bastard:
        return QnResourceTreeModelNodePtr();

    case NodeType::localResources:
        if (m_scope != FullScope)
            return bastardNode;
        return rootNode;

    case NodeType::users:
        if (m_scope == UsersScope)
            return QnResourceTreeModelNodePtr();    /*< Be the root node in this scope. */
        if (m_scope == FullScope && isAdmin)
            return rootNode;
        return bastardNode;

    case NodeType::otherSystems:
        if (m_scope == FullScope)
            return rootNode;
        return bastardNode;

    case NodeType::servers:
        if (m_scope == CamerasScope && isAdmin)
            return QnResourceTreeModelNodePtr();    /*< Be the root node in this scope. */
        if (m_scope == FullScope && isAdmin && m_systemHasManyServers)
            return rootNode;
        return bastardNode;

    case NodeType::localSeparator:
        if (m_scope == FullScope && !isLoggedIn)
            return rootNode;
        return bastardNode;

    case NodeType::currentSystem:
    case NodeType::currentUser:
    case NodeType::separator:
    case NodeType::layouts:
    case NodeType::layoutTours:
    case NodeType::webPages:
    case NodeType::analyticsEngines:
        if (m_scope == FullScope && isLoggedIn)
            return rootNode;
        return bastardNode;

    case NodeType::filteredCameras:
    case NodeType::filteredServers:
    case NodeType::filteredLayouts:
    case NodeType::filteredUsers:
    case NodeType::filteredVideowalls:
        return rootNode; // Always root node.

    case NodeType::userResources:
        if (m_scope == CamerasScope && !isAdmin)
            return QnResourceTreeModelNodePtr(); /*< Be the root node in this scope. */
        if (m_scope == FullScope && isLoggedIn && !isAdmin)
            return rootNode;
        return bastardNode;

    case NodeType::edge:
    {
        /* Only admins can see edge nodes. */
        if (!isAdmin || m_scope == UsersScope)
            return bastardNode;

        if (m_scope == CamerasScope)
            return m_rootNodes[NodeType::servers];

        NX_ASSERT(m_scope == FullScope);
        return m_systemHasManyServers
            ? m_rootNodes[NodeType::servers]
            : rootNode;
    }

    case NodeType::resource:
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
    auto rootNode = m_rootNodes[NodeType::root];
    auto bastardNode = m_rootNodes[NodeType::bastard];
    bool isAdmin = accessController()->hasGlobalPermission(GlobalPermission::admin);

    if (!node->resource())
        return bastardNode;

    /* No users nodes must be created here. */
    NX_ASSERT(!node->resourceFlags().testFlag(Qn::user));
    if (node->resourceFlags().testFlag(Qn::user))
        return bastardNode;

    /* In UsersScope all other nodes should be hidden. */
    if (m_scope == UsersScope)
        return bastardNode;

    if (node->resourceFlags().testFlag(Qn::server))
    {
        if (!isAdmin)
            return bastardNode;

        return m_systemHasManyServers || m_scope == CamerasScope
            ? m_rootNodes[NodeType::servers]
            : rootNode;
    }

    if (node->resourceFlags().testFlag(Qn::videowall))
    {
        if (m_scope != FullScope)
            return bastardNode;
        return m_rootNodes[NodeType::root];
    }

    if (node->resourceFlags().testFlag(Qn::web_page))
    {
        if (m_scope != FullScope)
            return bastardNode;
        return m_rootNodes[NodeType::webPages];
    }

    if (QnLayoutResourcePtr layout = node->resource().dynamicCast<QnLayoutResource>())
    {
        if (layout->isFile())
        {
            if (isLoggedIn)
                return m_rootNodes[NodeType::localResources];
            return rootNode;
        }

        if (layout->isShared())
            return m_rootNodes[NodeType::layouts];

        QnUserResourcePtr owner = layout->getParentResource().dynamicCast<QnUserResource>();
        if (!owner || owner == context()->user())
            return m_rootNodes[NodeType::layouts];

        return bastardNode;
    }

    /* Checking local resources. */
    auto parentResource = node->resource()->getParentResource();
    if (!parentResource || parentResource->flags().testFlag(Qn::local_server))
    {
        if (node->resourceFlags().testFlag(Qn::local))
        {
            if (isLoggedIn)
                return m_rootNodes[NodeType::localResources];
            return rootNode;
        }
        return bastardNode;
    }

    if (QnVirtualCameraResourcePtr camera = node->resource().dynamicCast<QnVirtualCameraResource>())
    {
        auto parentNode = isAdmin
            ? ensureResourceNode(parentResource)
            : bastardNode;

        QString groupId = camera->getGroupId();
        if (!groupId.isEmpty())
            return ensureRecorderNode(parentNode, camera);

        return parentNode;
    }

    if (const auto engine = node->resource().dynamicCast<AnalyticsEngineResource>())
        return m_rootNodes[NodeType::analyticsEngines];

    NX_ASSERT(false, "Should never get here.");
    return bastardNode;
}

void QnResourceTreeModel::updateNodeParent(const QnResourceTreeModelNodePtr& node)
{
    node->setParent(expectedParent(node));
}

void QnResourceTreeModel::updateNodeResource(const QnResourceTreeModelNodePtr& node,
    const QnResourcePtr& resource)
{
    if (node->resource() == resource)
        return;

    if (node->resource())
        m_nodesByResource[node->resource()].removeAll(node);
    node->setResource(resource);
    if (resource)
        m_nodesByResource[resource].push_back(node);
}

ResourceTreeNodeType QnResourceTreeModel::rootNodeTypeForScope() const
{
    switch (m_scope)
    {
    case QnResourceTreeModel::CamerasScope:
        return accessController()->hasGlobalPermission(GlobalPermission::admin)
            ? NodeType::servers
            : NodeType::userResources;
    case QnResourceTreeModel::UsersScope:
        return NodeType::users;
    default:
        return NodeType::root;
    }
}

QnResourceTreeModelCustomColumnDelegate* QnResourceTreeModel::customColumnDelegate() const
{
    return m_customColumnDelegate.data();
}

void QnResourceTreeModel::setCustomColumnDelegate(QnResourceTreeModelCustomColumnDelegate* columnDelegate)
{
    if (m_customColumnDelegate == columnDelegate)
        return;

    if (m_customColumnDelegate)
        disconnect(m_customColumnDelegate, nullptr, this, nullptr);

    m_customColumnDelegate = columnDelegate;

    auto notifyCustomColumnChanged = [this]()
        {
            // TODO: #GDM update only custom column and changed rows
            const auto root = m_rootNodes[rootNodeTypeForScope()];
            NX_ASSERT(root, lit("Absent root for scope %1: type of %2")
                .arg(m_scope).arg((int)rootNodeTypeForScope()));
            if (root)
                root->updateRecursive();
        };

    if (m_customColumnDelegate)
    {
        connect(m_customColumnDelegate, &QnResourceTreeModelCustomColumnDelegate::notifyDataChanged,
                this, notifyCustomColumnChanged);
    }

    notifyCustomColumnChanged();

}

QnResourceTreeModel::Scope QnResourceTreeModel::scope() const
{
    return m_scope;
}

QnResourceTreeModelNodePtr QnResourceTreeModel::rootNode(NodeType nodeType) const
{
    return m_rootNodes[nodeType];
}

// -------------------------------------------------------------------------- //
// QnResourceTreeModel :: QAbstractItemModel implementation
// -------------------------------------------------------------------------- //
QModelIndex QnResourceTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) /* hasIndex calls rowCount and columnCount. */
        return QModelIndex();

    return node(parent)->child(row)->createIndex(row, column);
}

QModelIndex QnResourceTreeModel::buddy(const QModelIndex& index) const
{
    return index;
}

QModelIndex QnResourceTreeModel::parent(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != this)
        return QModelIndex();
    return node(index)->parent()->createIndex(Qn::NameColumn);
}

bool QnResourceTreeModel::hasChildren(const QModelIndex& parent) const
{
    return rowCount(parent) > 0;
}

int QnResourceTreeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > Qn::NameColumn)
        return 0;

    return node(parent)->children().size();
}

int QnResourceTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return Qn::ColumnCount;
}

Qt::ItemFlags QnResourceTreeModel::flags(const QModelIndex& index) const
{
    if(!index.isValid())
        return Qt::NoItemFlags;

    if (index.column() == Qn::CustomColumn)
        return m_customColumnDelegate ? m_customColumnDelegate->flags(index) : Qt::NoItemFlags;

    return node(index)->flags(index.column());
}

QVariant QnResourceTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    if (role == Qn::ResourceTreeScopeRole)
        return qVariantFromValue(m_scope);

    /* Only standard QT roles are subject to reimplement. Otherwise we may go to recursion, getting resource for example. */
    if (index.column() == Qn::CustomColumn && role <= Qt::UserRole)
        return m_customColumnDelegate ? m_customColumnDelegate->data(index, role) : QVariant();

    return node(index)->data(role, index.column());
}

bool QnResourceTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
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
    roles.insert(Qn::ResourceStatusRole,        "status");
    roles.insert(Qn::NodeTypeRole,              "nodeType");
    return roles;
}

QStringList QnResourceTreeModel::mimeTypes() const
{
    auto result = MimeData::mimeTypes();
    result.append(pureTreeResourcesOnlyMimeType);
    return result;
}

QMimeData* QnResourceTreeModel::mimeData(const QModelIndexList& indexes) const
{
    QScopedPointer<QMimeData> baseMimeData(base_type::mimeData(indexes));
    if (!baseMimeData)
        return nullptr;

    /*
     * This flag services the assertion that cameras can be dropped on server
     * if and only if they are dragged from tree (not from layouts)
     */
    bool pureTreeResourcesOnly = true;

    QSet<QnUuid> entities;
    QSet<QnResourcePtr> resources;

    for (const auto& index: indexes)
    {
        auto node = this->node(index);
        if (!node)
            continue;

        switch (node->type())
        {
            case ResourceTreeNodeType::recorder:
            {
                for (auto child: node->children())
                {
                    if (auto res = child->resource())
                        resources.insert(res);
                }
                break;
            }
            case ResourceTreeNodeType::videoWallItem:
            case ResourceTreeNodeType::layoutTour:
                entities << node->uuid();
                break;
            case ResourceTreeNodeType::layoutItem:
                pureTreeResourcesOnly = false;
                break;
            default:
                break;
        }

        if (auto res = node->resource())
            resources.insert(res);
    }

    MimeData data(baseMimeData.get(), resourcePool());
    data.setEntities(entities.toList());
    data.setResources(resources.toList());
    data.setData(pureTreeResourcesOnlyMimeType, QByteArray(pureTreeResourcesOnly ? "1" : "0"));
    return data.createMimeData();
}

QnResourceTreeModelNodePtr QnResourceTreeModel::dropNode(const QMimeData* mimeData,
    int row, int column, const QModelIndex& parent, bool checkOnly) const
{
    if (!mimeData)
        return {};

    // Check where we're dropping it.
    auto node = this->node(parent);
    NX_ASSERT(node);
    if (!node)
        return {};

    auto check =
        [](const QnResourceTreeModelNodePtr& node, Qn::ResourceFlags flags)
        {
            //NX_ASSERT(node && node->resource() && node->resource()->hasFlags(flags));
            auto result = node && node->resource() && node->resource()->hasFlags(flags);
            TRACE("Check drop on " << node->data(Qt::DisplayRole, 0).toString() << "result " <<
                result);
            return result;
        };

    TRACE((checkOnly ? "Checking if drop allowed on the node" : "Dropping on the node ")
        << node->data(Qt::DisplayRole, 0).toString() << " type " << (int) node->type());

    // There are some drag&drop behaviours, that require to move up through the tree

    // Dropping into an item is the same as dropping into a layout.
    if (node->type() == ResourceTreeNodeType::layoutItem)
    {
        node = node->parent();
        if (!check(node, Qn::layout))
            return {};
    }

    // Dropping into accessible layouts is the same as dropping into a user.
    if (node->type() == ResourceTreeNodeType::sharedLayouts)
    {
        node = node->parent();
        if (node->type() != ResourceTreeNodeType::role && !check(node, Qn::user))
            return {};
    }

    // Dropping into a server camera is the same as dropping into a server.
    if (node->parent() && (node->parent()->resourceFlags().testFlag(Qn::server)))
    {
        node = node->parent();
        if (!check(node, Qn::server))
            return {};
    }

    // Dropping something into a group of resources.
    // Dropping at a group of cameras -> ResourceTreeNodeType::sharedResources
    // Dropping at a group of layouts -> ResourceTreeNodeType::sharedLayouts
    // In both cases we expect that parent node is user or user role.
    if (node->type() == ResourceTreeNodeType::sharedResources
        || node->type() == ResourceTreeNodeType::sharedLayouts)
    {
        node = node->parent();
        if (!node)
            return {};
        auto parentType = node->type();
        if (parentType != ResourceTreeNodeType::role
            && parentType != ResourceTreeNodeType::resource)
        {
            return {};
        }
    }

    return node;
}

bool QnResourceTreeModel::canDropMimeData(const QMimeData* mimeData, Qt::DropAction action,
    int row, int column, const QModelIndex& parent) const
{
    if (!mimeData)
        return false;

    // Check if the action is supported.
    if (!supportedDropActions().testFlag(action))
        return false;

    // Check if the format is supported.
    if (!intersects(mimeData->formats(), MimeData::mimeTypes()))
        return base_type::canDropMimeData(mimeData, action, row, column, parent);

    // Check if the drop is targeted for the scene.
    MimeData data(mimeData, resourcePool());
    if (data.arguments().contains(Qn::ItemTimeRole))
        return false;

    auto node = dropNode(mimeData, row, column, parent, true /*checkOnly*/);
    if (!node)
        return false;

    // TODO: #vkutin #gdm Add actual check if supplied entites/resources can be dropped on the node.

    return true;
}

bool QnResourceTreeModel::dropMimeData(const QMimeData* mimeData, Qt::DropAction action,
    int row, int column, const QModelIndex& parent)
{
    if (!mimeData)
        return false;

    // Check if the action is supported.
    if (!supportedDropActions().testFlag(action))
        return false;

    // Check if the format is supported.
    if (!intersects(mimeData->formats(), MimeData::mimeTypes()))
        return base_type::dropMimeData(mimeData, action, row, column, parent);

    auto node = dropNode(mimeData, row, column, parent, false /*checkOnly*/);
    if (!node)
        return false;

    // Decode.
    // Resource tree drop is working only for resources that are already in the pool.
    MimeData data(mimeData, resourcePool());
    resourcePool()->addNewResources(data.resources());

    // Drop on videowall is handled in videowall.
    if (node->type() == ResourceTreeNodeType::videoWallItem)
    {
        const auto videoWallItems = resourcePool()->getVideoWallItemsByUuid(data.entities());

        action::Parameters parameters;
        if (!videoWallItems.empty())
        {
            parameters = videoWallItems;
        }
        else
        {
            parameters = data.resources();
        }
        parameters.setArgument(Qn::VideoWallItemGuidRole, node->uuid());
        menu()->trigger(action::DropOnVideoWallItemAction, parameters);
    }
    else if (node->type() == ResourceTreeNodeType::role)
    {
        auto layoutsToShare = data.resources().filtered<QnLayoutResource>(
            [](const QnLayoutResourcePtr& layout)
            {
                return !layout->isFile();
            });
        QnUuid roleId = node->uuid();
        for (const auto& layout: layoutsToShare)
        {
            TRACE("Sharing layout " << layout->getName() << " with role "
                << node->m_displayName);

            menu()->trigger(action::ShareLayoutAction,
                action::Parameters(layout).withArgument(Qn::UuidRole, roleId));
        }
        auto camerasToShare = data.resources().filtered<QnVirtualCameraResource>();
        for (const auto& camera: camerasToShare)
        {
            TRACE("Sharing camera " << camera->getName() << " with role "
                << node->m_displayName);

            menu()->trigger(action::ShareCameraAction,
                action::Parameters(camera).withArgument(Qn::UuidRole, roleId));
        }
    }
    else
    {
        handleDrop(data.resources(), node->resource(), mimeData);
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
void QnResourceTreeModel::at_resPool_resourceAdded(const QnResourcePtr& resource)
{
    if (m_scope == UsersScope)
        return;

    NX_ASSERT(resource);
    if (!resource)
        return;

    // We can get here while checking event loop on application closing
    if (!resource->resourcePool())
        return;

    if (resource->hasFlags(Qn::fake))
        return;

    if (resource->hasFlags(Qn::user))
        return;

    if (resource.dynamicCast<QnStorageResource>())
        return;

    // Ignore "Preview Search" layouts.
    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        // TODO: #GDM do not add preview search layouts to the resource pool
        if (layout->data().contains(Qn::LayoutSearchStateRole))
            return;
    }

    // Skip cameras inside the exported layouts. Only layout items are to be displayed there.
    const bool isExportedCamera = resource->hasFlags(Qn::local_video)
        && !resource->getParentId().isNull();
    if (isExportedCamera)
        return;

    connect(resource, &QnResource::parentIdChanged, this,
        &QnResourceTreeModel::at_resource_parentIdChanged);

    QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
    if (videoWall)
    {
        connect(videoWall,  &QnVideoWallResource::itemAdded,            this,   &QnResourceTreeModel::at_videoWall_itemAdded);
        connect(videoWall,  &QnVideoWallResource::itemChanged,          this,   &QnResourceTreeModel::at_videoWall_itemAdded);
        connect(videoWall,  &QnVideoWallResource::itemRemoved,          this,   &QnResourceTreeModel::at_videoWall_itemRemoved);

        connect(videoWall,  &QnVideoWallResource::matrixAdded,          this,   &QnResourceTreeModel::at_videoWall_matrixAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::matrixChanged,        this,   &QnResourceTreeModel::at_videoWall_matrixAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::matrixRemoved,        this,   &QnResourceTreeModel::at_videoWall_matrixRemoved);
    }

    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (camera)
    {
        connect(camera, &QnVirtualCameraResource::groupIdChanged,   this, &QnResourceTreeModel::at_resource_parentIdChanged);

        auto updateParent = [this](const QnResourcePtr &resource)
            {
                /* Automatically update display name of the EDGE server if its camera was renamed. */
                QnResourcePtr parent = resource->getParentResource();
                if (QnMediaServerResource::isEdgeServer(parent))
                {
                    for (auto node: m_nodesByResource.value(parent))
                        node->update();
                }
            };
        connect(camera, &QnResource::nameChanged, this, updateParent);
        updateParent(camera);
    }

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (server)
    {
        connect(server, &QnMediaServerResource::redundancyChanged, this,
            &QnResourceTreeModel::at_server_redundancyChanged);
    }

    if (const auto webPage = resource.dynamicCast<QnWebPageResource>())
    {
        connect(webPage, &QnWebPageResource::subtypeChanged, this,
            [this](const QnWebPageResourcePtr& webPage)
            {
                for (auto node: m_nodesByResource.value(webPage))
                    node->updateIcon();
            });
    }

    auto node = ensureResourceNode(resource);
    updateNodeParent(node);

    if (server)
    {
        for (const QnResourcePtr &camera: resourcePool()->getResourcesByParentId(server->getId()))
        {
            if (m_resourceNodeByResource.contains(camera))
                at_resource_parentIdChanged(camera);
        }
        updateSystemHasManyServers();
    }

    if (videoWall)
    {
        for (const QnVideoWallItem &item : videoWall->items()->getItems())
            at_videoWall_itemAdded(videoWall, item);
        for (const QnVideoWallMatrix &matrix : videoWall->matrices()->getItems())
            at_videoWall_matrixAddedOrChanged(videoWall, matrix);
    }
}

void QnResourceTreeModel::at_resPool_resourceRemoved(const QnResourcePtr& resource)
{
    if (!resource)
        return;

    resource->disconnect(this);

    QList<QnResourceTreeModelNodePtr> nodesToDelete;
    for (auto node: m_allNodes)
    {
        if (node->resource() != resource)
            continue;

        if (node->type() == ResourceTreeNodeType::videoWallItem)
            updateNodeResource(node, QnResourcePtr());
        else
            nodesToDelete << node;
    }

    for (auto node: nodesToDelete)
        removeNode(node);

    m_nodesByResource.remove(resource);
    m_resourceNodeByResource.remove(resource);

    if (resource->hasFlags(Qn::server))
        updateSystemHasManyServers();
}


void QnResourceTreeModel::rebuildTree()
{
    // TODO: #vkutin #gdm Implement "model reset" logic for the tree.
    // Currently it's not handled, "rows inserted/removed" is handled instead.

    m_rootNodes[ResourceTreeNodeType::currentUser]->setResource(context()->user());

    // Force re-create camera nodes for edge servers.
    for (const auto& resource: commonModule()->resourcePool()->getAllServers(Qn::AnyStatus))
    {
        if (QnMediaServerResource::isHiddenServer(resource))
            at_server_redundancyChanged(resource);
    }

    for (auto nodeType: rootNodeTypes())
    {
        auto node = m_rootNodes[nodeType];
        if (!node)
            continue;

        updateNodeParent(node);
        node->update();
    }

    for (auto node: m_resourceNodeByResource)
    {
        updateNodeParent(node);
        node->update();
    }
}

void QnResourceTreeModel::handleDrop(
    const QnResourceList& sourceResources,
    const QnResourcePtr& targetResource,
    const QMimeData* mimeData)
{
    if (sourceResources.isEmpty() || !targetResource)
        return;

    /* We can add media resources to layout */
    if (const auto layout = targetResource.dynamicCast<QnLayoutResource>())
    {
        const auto droppable = sourceResources.filtered(QnResourceAccessFilter::isOpenableInLayout);

        if (!droppable.isEmpty())
        {
            menu()->trigger(action::OpenInLayoutAction, action::Parameters(droppable)
                .withArgument(Qn::LayoutResourceRole, layout));
            menu()->trigger(action::SaveLayoutAction, layout);
        }
    }

    /* Drop layout on user means sharing this layout. */
    else if (const auto targetUser = targetResource.dynamicCast<QnUserResource>())
    {
        /* Technically it works right, but layout becomes shared and appears in "Shared layouts"
         * node, not under user, where it was dragged. Disabling to not confuse user. */
        if (targetUser->userRole() == Qn::UserRole::customUserRole)
            return;

        for (const auto& sourceLayout: sourceResources.filtered<QnLayoutResource>())
        {
            if (sourceLayout->isFile())
                continue;

            TRACE("Sharing layout " << sourceLayout->getName() << " with " << targetUser->getName())
            menu()->trigger(action::ShareLayoutAction, action::Parameters(sourceLayout)
                .withArgument(Qn::UserResourceRole, targetUser));
        }

        for (const auto& sourceCamera: sourceResources.filtered<QnVirtualCameraResource>())
        {
            TRACE("Sharing camera " << sourceCamera->getName() << " with " << targetUser->getName());

            menu()->trigger(action::ShareCameraAction, action::Parameters(sourceCamera)
                .withArgument(Qn::UserResourceRole, targetUser));
        }
    }

    /* Drop camera on server allows to move servers between cameras. */
    else if (const auto server = targetResource.dynamicCast<QnMediaServerResource>())
    {
        if (server->hasFlags(Qn::fake))
            return;

        /* Do not allow to drop camera items from layouts. */
        if (mimeData->data(pureTreeResourcesOnlyMimeType) != QByteArray("1"))
            return;

        const auto cameras = sourceResources.filtered<QnVirtualCameraResource>();
        if (!cameras.empty())
        {
            menu()->trigger(action::MoveCameraAction, action::Parameters(cameras)
                .withArgument(Qn::MediaServerResourceRole, server));
        }
    }
}

void QnResourceTreeModel::handlePermissionsChanged(const QnResourcePtr& resource)
{
    if (m_resourceNodeByResource.contains(resource))
    {
        for (auto node: m_nodesByResource[resource])
            node->updateRecursive();
    }
}

void QnResourceTreeModel::updateSystemHasManyServers()
{
    const auto servers = resourcePool()->getAllServers(Qn::AnyStatus);
    const bool hasManyServers = servers.size() > 1;
    if (m_systemHasManyServers == hasManyServers)
        return;

    m_systemHasManyServers = hasManyServers;
    for (const auto& server: servers)
    {
        if (QnMediaServerResource::isHiddenServer(server))
        {
            for (const auto& camera: resourcePool()->getAllCameras(server, true))
            {
                auto node = ensureResourceNode(camera);
                updateNodeParent(node);
            }
        }
        else
        {
            auto node = ensureResourceNode(server);
            updateNodeParent(node);
        }
    }

    updateNodeParent(m_rootNodes[ResourceTreeNodeType::servers]);
}

void QnResourceTreeModel::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &layout)
{
    if (auto videowall = layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>())
    {
        bool modified = snapshotManager()->isModified(layout);
        ensureResourceNode(videowall)->setModified(modified);
        return;
    }
}

void QnResourceTreeModel::at_resource_parentIdChanged(const QnResourcePtr &resource)
{
    auto node = ensureResourceNode(resource);

    /* Update edge resource if we are the admin. */
    if (accessController()->hasGlobalPermission(GlobalPermission::admin))
    {
        if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        {
            auto server = camera->getParentServer();
            bool wasEdge = (node->type() == ResourceTreeNodeType::edge);
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
}

void QnResourceTreeModel::at_videoWall_itemAdded(const QnVideoWallResourcePtr& videoWall, const QnVideoWallItem& item)
{
    auto parentNode = ensureResourceNode(videoWall);
    auto node = ensureItemNode(parentNode, item.uuid, ResourceTreeNodeType::videoWallItem);

    QnResourcePtr resource;
    if (!item.layout.isNull())
        resource = resourcePool()->getResourceById(item.layout);

    if (node->resource() != resource)
        updateNodeResource(node, resource);
    else
        node->update(); // in case of _changed method call, where setResource will exit instantly
}

void QnResourceTreeModel::at_videoWall_itemRemoved(const QnVideoWallResourcePtr& videoWall, const QnVideoWallItem& item)
{
    auto parentNode = ensureResourceNode(videoWall);
    auto node = m_itemNodesByParent[parentNode].take(item.uuid);
    if (node)
        removeNode(node);
}

void QnResourceTreeModel::at_videoWall_matrixAddedOrChanged(const QnVideoWallResourcePtr& videoWall, const QnVideoWallMatrix& matrix)
{
    auto parentNode = ensureResourceNode(videoWall);
    auto node = ensureItemNode(parentNode, matrix.uuid, ResourceTreeNodeType::videoWallMatrix);
    node->update(); // TODO: #GDM what for?
}

void QnResourceTreeModel::at_videoWall_matrixRemoved(const QnVideoWallResourcePtr& videoWall, const QnVideoWallMatrix& matrix)
{
    auto parentNode = ensureResourceNode(videoWall);
    auto node = m_itemNodesByParent[parentNode].take(matrix.uuid);
    if (node)
        removeNode(node);
}

void QnResourceTreeModel::at_server_redundancyChanged(const QnResourcePtr& resource)
{
    auto node = ensureResourceNode(resource);
    node->update();

    /* Update edge nodes if we are the admin. */
    if (accessController()->hasGlobalPermission(GlobalPermission::admin))
    {
        for (const QnVirtualCameraResourcePtr &cameraResource: resourcePool()->getAllCameras(resource, true))
        {
            auto existingNode = m_resourceNodeByResource.take(cameraResource);
            removeNode(existingNode);

            /* Re-create node as it should change its NodeType from ResourceNode to EdgeNode or vice versa. */
            auto updatedNode = ensureResourceNode(cameraResource);
            updateNodeParent(updatedNode);
        }
    }
}

void QnResourceTreeModel::at_systemNameChanged()
{
    m_rootNodes[ResourceTreeNodeType::currentSystem]->update();
}

void QnResourceTreeModel::at_autoDiscoveryEnabledChanged()
{
    m_rootNodes[ResourceTreeNodeType::otherSystems]->update();
}

void QnResourceTreeModel::at_wearableManager_stateChanged(const WearableState& state)
{
    QnResourcePtr resource = resourcePool()->getResourceById(state.cameraId);
    if (!resource)
        return; //< Resource was removed while signal was in flight.

    auto node = ensureResourceNode(resource);
    node->update();
}

QnResourceTreeModelNodeManager* QnResourceTreeModel::nodeManager() const
{
    return m_nodeManager;
}

QnResourceTreeModelLayoutNodeManager* QnResourceTreeModel::layoutNodeManager() const
{
    return m_layoutNodeManager;
}
