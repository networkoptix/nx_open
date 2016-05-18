#include "resource_tree_model.h"
#include <cassert>

#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>

#include <utils/common/checked_cast.h>
#include <common/common_meta_types.h>
#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_matrix.h>

#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_pc_data.h>

#include <api/global_settings.h>

#include <core/resource_management/resource_pool.h>
#include <ui/actions/action_manager.h>
#include <ui/common/ui_resource_name.h>
#include <ui/delegates/resource_tree_model_custom_column_delegate.h>
#include <ui/models/resource/resource_tree_model_node.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_resource.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_access_controller.h>

namespace {
    const char *pureTreeResourcesOnlyMimeType = "application/x-noptix-pure-tree-resources-only";

    bool intersects(const QStringList &l, const QStringList &r) {
        foreach(const QString &s, l)
            if(r.contains(s))
                return true;
        return false;
    }

    Qn::NodeType rootNodeTypeForScope(QnResourceTreeModel::Scope scope) {
        switch (scope) {
        case QnResourceTreeModel::CamerasScope:
            return Qn::ServersNode;
        case QnResourceTreeModel::UsersScope:
            return Qn::UsersNode;
        default:
            return Qn::RootNode;
        }
    }

} // namespace

// -------------------------------------------------------------------------- //
// QnResourceTreeModel :: contructors, destructor and helpers.
// -------------------------------------------------------------------------- //
QnResourceTreeModel::QnResourceTreeModel(Scope scope, QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_urlsShown(true),
    m_scope(scope)
{
    m_rootNodeTypes
        << Qn::LocalNode
        << Qn::UsersNode
        << Qn::ServersNode
        << Qn::WebPagesNode
        << Qn::OtherSystemsNode
        << Qn::RootNode
        << Qn::BastardNode;

    /* Create top-level nodes. */
    foreach(Qn::NodeType t, m_rootNodeTypes) {
        m_rootNodes[t] = new QnResourceTreeModelNode(this, t);
        m_allNodes.append(m_rootNodes[t]);
    }


    Qn::NodeType parentNodeType = scope == FullScope ? Qn::RootNode : Qn::BastardNode;
    Qn::NodeType rootNodeType = rootNodeTypeForScope(m_scope);

    foreach(Qn::NodeType t, m_rootNodeTypes)
        if (t != rootNodeType && t != parentNodeType)
            m_rootNodes[t]->setParent(m_rootNodes[parentNodeType]);

    /* Connect to context. */
    connect(resourcePool(),     &QnResourcePool::resourceAdded,                     this,   &QnResourceTreeModel::at_resPool_resourceAdded, Qt::QueuedConnection);
    connect(resourcePool(),     &QnResourcePool::resourceRemoved,                   this,   &QnResourceTreeModel::at_resPool_resourceRemoved, Qt::QueuedConnection);
    connect(snapshotManager(),  &QnWorkbenchLayoutSnapshotManager::flagsChanged,    this,   &QnResourceTreeModel::at_snapshotManager_flagsChanged);
    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged,   this,   &QnResourceTreeModel::at_accessController_permissionsChanged);
    connect(context(),          &QnWorkbenchContext::userChanged,                   this,   &QnResourceTreeModel::rebuildTree, Qt::QueuedConnection);
    connect(qnCommon,           &QnCommonModule::systemNameChanged,                 this,   &QnResourceTreeModel::at_commonModule_systemNameChanged);
    connect(qnCommon,           &QnCommonModule::readOnlyChanged,                   this,   &QnResourceTreeModel::rebuildTree, Qt::QueuedConnection);
    connect(QnGlobalSettings::instance(),   &QnGlobalSettings::serverAutoDiscoveryChanged,  this,   &QnResourceTreeModel::at_serverAutoDiscoveryEnabledChanged);

    QnResourceList resources = resourcePool()->getResources();

    rebuildTree();

    /* It is important to connect before iterating as new resources may be added to the pool asynchronously. */
    foreach(const QnResourcePtr &resource, resources)
        at_resPool_resourceAdded(resource);

}

QnResourceTreeModel::~QnResourceTreeModel() {
    /* Disconnect from context. */
    disconnect(resourcePool(), NULL, this, NULL);
    disconnect(snapshotManager(), NULL, this, NULL);

    /* Make sure all nodes will have their parent empty. */
    foreach(QnResourceTreeModelNode* node, m_allNodes)
        node->clear();

    /* Free memory. */
    qDeleteAll(m_resourceNodeByResource);
    qDeleteAll(m_itemNodeByUuid);
    qDeleteAll(m_systemNodeBySystemName);
    foreach(Qn::NodeType t, m_rootNodeTypes) {
        delete m_rootNodes[t];
        qDeleteAll(m_nodes[t]);
    }

    /* Note, we don't have to check and remove all the resources from the resource pool
       because they will be removed recursively starting from root nodes. */
}

QnResourcePtr QnResourceTreeModel::resource(const QModelIndex &index) const {
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

QnResourceTreeModelNode *QnResourceTreeModel::node(const QnResourcePtr &resource) {
    if(!resource)
        return m_rootNodes[Qn::BastardNode];

    QHash<QnResourcePtr, QnResourceTreeModelNode *>::iterator pos = m_resourceNodeByResource.find(resource);
    if(pos == m_resourceNodeByResource.end()) {
        Qn::NodeType nodeType = Qn::ResourceNode;
        if (QnMediaServerResource::isHiddenServer(resource->getParentResource()))
            nodeType = Qn::EdgeNode;

        pos = m_resourceNodeByResource.insert(resource, new QnResourceTreeModelNode(this, resource, nodeType));
        m_allNodes.append(*pos);
    }
    return *pos;
}

QnResourceTreeModelNode *QnResourceTreeModel::node(Qn::NodeType nodeType, const QnUuid &uuid, const QnResourcePtr &resource) {
    NodeKey key(resource, uuid);
    if (!m_nodes[nodeType].contains(key)) {
        QnResourceTreeModelNode* node = new QnResourceTreeModelNode(this, uuid, nodeType);
        m_nodes[nodeType].insert(key, node);
        m_allNodes.append(node);
        return node;
    }
    return m_nodes[nodeType][key];
}

QnResourceTreeModelNode *QnResourceTreeModel::node(const QnUuid &uuid) {
    QHash<QnUuid, QnResourceTreeModelNode *>::iterator pos = m_itemNodeByUuid.find(uuid);
    if(pos == m_itemNodeByUuid.end()) {
        pos = m_itemNodeByUuid.insert(uuid, new QnResourceTreeModelNode(this, uuid));
        m_allNodes.append(*pos);
    }
    return *pos;
}

QnResourceTreeModelNode *QnResourceTreeModel::node(const QModelIndex &index) const {
    /* Root node. */
    if(!index.isValid())
        return m_rootNodes[rootNodeTypeForScope(m_scope)];

    return static_cast<QnResourceTreeModelNode *>(index.internalPointer());
}

QnResourceTreeModelNode *QnResourceTreeModel::node(const QnResourcePtr &resource, const QString &groupId, const QString &groupName) {
    if (m_recorderHashByResource[resource].contains(groupId))
        return m_recorderHashByResource[resource][groupId];

    QnResourceTreeModelNode* recorder = new QnResourceTreeModelNode(this, Qn::RecorderNode, groupName);
    if (m_scope != UsersScope)
        recorder->setParent(m_resourceNodeByResource[resource]);
    else
        recorder->setParent(m_rootNodes[Qn::BastardNode]);
    m_recorderHashByResource[resource][groupId] = recorder;
    m_allNodes.append(recorder);
    return recorder;
}

QnResourceTreeModelNode *QnResourceTreeModel::systemNode(const QString &systemName) {
    QnResourceTreeModelNode *node = m_systemNodeBySystemName[systemName];
    if (node)
        return node;

    node = new QnResourceTreeModelNode(this, Qn::SystemNode, systemName);
    if (m_scope == FullScope)
        node->setParent(m_rootNodes[Qn::OtherSystemsNode]);
    else
        node->setParent(m_rootNodes[Qn::BastardNode]);
    m_systemNodeBySystemName[systemName] = node;
    m_allNodes.append(node);
    return node;
}

void QnResourceTreeModel::removeNode(QnResourceTreeModelNode *node) {
    switch(node->type()) {
    case Qn::VideoWallItemNode:
    case Qn::VideoWallMatrixNode:
        removeNode(node->type(), node->uuid(), node->resource());
        return;

    case Qn::ResourceNode:
    case Qn::EdgeNode:
        m_resourceNodeByResource.remove(node->resource());
        break;
    case Qn::ItemNode:
        m_itemNodeByUuid.remove(node->uuid());
        if (node->resource() && m_itemNodesByResource.contains(node->resource()))
            m_itemNodesByResource[node->resource()].removeAll(node);
        break;
    case Qn::RecorderNode:
        break;  //nothing special
    case Qn::SystemNode:
        break;  //nothing special
    default:
        NX_ASSERT(false, Q_FUNC_INFO, "should never get here");
    }

    deleteNode(node);
}

void QnResourceTreeModel::removeNode(Qn::NodeType nodeType, const QnUuid &uuid, const QnResourcePtr &resource) {
    NodeKey key(resource, uuid);
    if (!m_nodes[nodeType].contains(key))
        return;

    deleteNode(m_nodes[nodeType].take(key));
}

void QnResourceTreeModel::deleteNode(QnResourceTreeModelNode *node) {
    NX_ASSERT(m_allNodes.contains(node));
    m_allNodes.removeOne(node);
    foreach (QnResourceTreeModelNode* existing, m_allNodes)
        if (existing->parent() == node)
            existing->setParent(NULL);

    delete node;
}

QnResourceTreeModelNode *QnResourceTreeModel::expectedParent(QnResourceTreeModelNode *node) {
    NX_ASSERT(node->type() == Qn::ResourceNode || node->type() == Qn::EdgeNode);

    if(!node->resource())
        return m_rootNodes[Qn::BastardNode];

    if(node->resourceFlags() & Qn::user) {
        if (m_scope == UsersScope)
            return m_rootNodes[Qn::UsersNode];
        if (m_scope == CamerasScope)
            return m_rootNodes[Qn::BastardNode];

        //TODO: #GDM non-admin scope?
        if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
            return m_rootNodes[Qn::RootNode];

        QnUserResourcePtr user = node->resource().dynamicCast<QnUserResource>();
        if (user && !user->isEnabled())
            return m_rootNodes[Qn::BastardNode];

        return m_rootNodes[Qn::UsersNode];
    }

    /* In UsersScope all other nodes should be hidden. */
    if (m_scope == UsersScope)
        return m_rootNodes[Qn::BastardNode];


    if (node->type() == Qn::EdgeNode)
        return m_rootNodes[Qn::ServersNode];

    if (node->resourceFlags() & Qn::server) {
        if (QnMediaServerResource::isFakeServer(node->resource())) {
            if (m_scope == CamerasScope)
                return m_rootNodes[Qn::BastardNode];

            QnMediaServerResourcePtr server = node->resource().staticCast<QnMediaServerResource>();
            QString systemName = server->getSystemName();
            if (systemName == qnCommon->localSystemName())
                return m_rootNodes[Qn::ServersNode];

            return systemName.isEmpty() ? m_rootNodes[Qn::OtherSystemsNode] : systemNode(systemName);
        }
        return m_rootNodes[Qn::ServersNode];
    }

    if (node->resourceFlags() & Qn::videowall) {
        if (m_scope == CamerasScope)
            return m_rootNodes[Qn::BastardNode];
        else
            return m_rootNodes[Qn::RootNode];
    }

    if (node->resourceFlags().testFlag(Qn::web_page))
    {
        if (m_scope == FullScope)
            return m_rootNodes[Qn::WebPagesNode];
        return m_rootNodes[Qn::BastardNode];
    }

    QnResourcePtr parentResource = resourcePool()->getResourceById(node->resource()->getParentId());
    if(!parentResource || (parentResource->flags() & Qn::local_server) == Qn::local_server) {
        if(node->resourceFlags() & Qn::local) {
            return m_rootNodes[Qn::LocalNode];
        } else {
            return m_rootNodes[Qn::BastardNode];
        }
    } else {
        QnResourceTreeModelNode* parent = this->node(parentResource);

        if (QnSecurityCamResourcePtr camera = node->resource().dynamicCast<QnSecurityCamResource>()) {
            QString groupName = camera->getGroupName();
            QString groupId = camera->getGroupId();
            if(!groupId.isEmpty())
                parent = this->node(parentResource, groupId, groupName.isEmpty() ? groupId : groupName);
        }

        return parent;
    }
}

bool QnResourceTreeModel::isUrlsShown() {
    return m_urlsShown;
}

void QnResourceTreeModel::setUrlsShown(bool urlsShown) {
    if(urlsShown == m_urlsShown)
        return;

    m_urlsShown = urlsShown;

    Qn::NodeType rootNodeType = rootNodeTypeForScope(m_scope);
    m_rootNodes[rootNodeType]->updateRecursive();
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

    auto notifyCustomColumnChanged = [this](){
        //TODO: #GDM update only custom column and changed rows
        Qn::NodeType rootNodeType = rootNodeTypeForScope(m_scope);
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
QModelIndex QnResourceTreeModel::index(int row, int column, const QModelIndex &parent) const {
    if(!hasIndex(row, column, parent)) /* hasIndex calls rowCount and columnCount. */
        return QModelIndex();

    return node(parent)->child(row)->index(row, column);
}

QModelIndex QnResourceTreeModel::buddy(const QModelIndex &index) const {
    return index;
}

QModelIndex QnResourceTreeModel::parent(const QModelIndex &index) const {
    if (!index.isValid() || index.model() != this)
        return QModelIndex();
    return node(index)->parent()->index(Qn::NameColumn);
}

bool QnResourceTreeModel::hasChildren(const QModelIndex &parent) const {
    return rowCount(parent) > 0;
}

int QnResourceTreeModel::rowCount(const QModelIndex &parent) const {
    if (parent.column() > Qn::NameColumn)
        return 0;

    return node(parent)->children().size();
}

int QnResourceTreeModel::columnCount(const QModelIndex &parent) const {
    return Qn::ColumnCount;
}

Qt::ItemFlags QnResourceTreeModel::flags(const QModelIndex &index) const {
    if(!index.isValid())
        return Qt::NoItemFlags;

    if (index.column() == Qn::CustomColumn)
        return m_customColumnDelegate ? m_customColumnDelegate->flags(index) : Qt::NoItemFlags;

    return node(index)->flags(index.column());
}

QVariant QnResourceTreeModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    /* Only standard QT roles are subject to reimplement. Otherwise we may go to recursion, getting resource for example. */
    if (index.column() == Qn::CustomColumn && role <= Qt::UserRole)
        return m_customColumnDelegate ? m_customColumnDelegate->data(index, role) : QVariant();

    return node(index)->data(role, index.column());
}

bool QnResourceTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if(!index.isValid())
        return false;

    if (index.column() == Qn::CustomColumn)
        return m_customColumnDelegate ? m_customColumnDelegate->setData(index, value, role) : false;

    return node(index)->setData(value, role, index.column());
}

QVariant QnResourceTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    Q_UNUSED(section);
    Q_UNUSED(orientation);
    Q_UNUSED(role);

    return QVariant(); /* No headers needed. */
}

QHash<int,QByteArray> QnResourceTreeModel::roleNames() const {
    QHash<int, QByteArray> roles = base_type::roleNames();
    roles.insert(Qn::ResourceRole,              "resource");
    roles.insert(Qn::ResourceFlagsRole,         "flags");
    roles.insert(Qn::ItemUuidRole,              "uuid");
    roles.insert(Qn::ResourceSearchStringRole,  "searchString");
    roles.insert(Qn::ResourceStatusRole,        "status");
    roles.insert(Qn::NodeTypeRole,              "nodeType");
    return roles;
}

QStringList QnResourceTreeModel::mimeTypes() const {
    QStringList result = QnWorkbenchResource::resourceMimeTypes();
    result.append(QLatin1String(pureTreeResourcesOnlyMimeType));
    result.append(QnVideoWallItem::mimeType());
    return result;
}

QMimeData *QnResourceTreeModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeData = base_type::mimeData(indexes);
    if (mimeData) {
        const QStringList types = mimeTypes();

        /*
         * This flag services the assertion that cameras can be dropped on mediaserver
         * if and only if they are dragged from tree (not from layouts)
         */
        bool pureTreeResourcesOnly = true;

        if (intersects(types, QnWorkbenchResource::resourceMimeTypes())) {
            QnResourceList resources;
            foreach (const QModelIndex &index, indexes) {
                QnResourceTreeModelNode *node = this->node(index);

                if (node && node->type() == Qn::RecorderNode) {
                    foreach (QnResourceTreeModelNode* child, node->children()) {
                        if (child->resource() && !resources.contains(child->resource()))
                            resources.append(child->resource());
                    }
                }

                if(node && node->resource() && !resources.contains(node->resource()))
                    resources.append(node->resource());

                if(node && (node->type() == Qn::ItemNode))
                    pureTreeResourcesOnly = false;
            }

            QnWorkbenchResource::serializeResources(resources, types, mimeData);
        }

        if (types.contains(QnVideoWallItem::mimeType())) {
            QSet<QnUuid> uuids;
            foreach (const QModelIndex &index, indexes) {
                QnResourceTreeModelNode *node = this->node(index);

                if (node && (node->type() == Qn::VideoWallItemNode)) {
                    uuids << node->uuid();
                }
            }
            QnVideoWallItem::serializeUuids(uuids.toList(), mimeData);

        }

        if(types.contains(QLatin1String(pureTreeResourcesOnlyMimeType)))
            mimeData->setData(QLatin1String(pureTreeResourcesOnlyMimeType), QByteArray(pureTreeResourcesOnly ? "1" : "0"));
    }

    return mimeData;
}

bool QnResourceTreeModel::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
    if (!mimeData)
        return false;

    /* Check if the action is supported. */
    if(!(action & supportedDropActions()))
        return false;

    /* Check if the format is supported. */
    if(!intersects(mimeData->formats(), QnWorkbenchResource::resourceMimeTypes()) && !mimeData->formats().contains(QnVideoWallItem::mimeType()))
        return base_type::dropMimeData(mimeData, action, row, column, parent);

    /* Decode. */
    QnResourceList resources = QnWorkbenchResource::deserializeResources(mimeData);

    /* Check where we're dropping it. */
    QnResourceTreeModelNode *node = this->node(parent);

    if(node->type() == Qn::ItemNode)
        node = node->parent(); /* Dropping into an item is the same as dropping into a layout. */

    if(node->parent() && (node->parent()->resourceFlags() & Qn::server))
        node = node->parent(); /* Dropping into a server item is the same as dropping into a server */

    if (node->type() == Qn::VideoWallItemNode) {
        QnActionParameters parameters;
        if (mimeData->hasFormat(QnVideoWallItem::mimeType()))
            parameters = QnActionParameters(qnResPool->getVideoWallItemsByUuid(QnVideoWallItem::deserializeUuids(mimeData)));
        else
            parameters = QnActionParameters(resources);
        parameters.setArgument(Qn::VideoWallItemGuidRole, node->uuid());
        menu()->trigger(QnActions::DropOnVideoWallItemAction, parameters);
    } else if(QnLayoutResourcePtr layout = node->resource().dynamicCast<QnLayoutResource>()) {
        QnResourceList medias;
        foreach( QnResourcePtr res, resources )
        {
            if( res.dynamicCast<QnMediaResource>() )
                medias.push_back( res );
        }

        menu()->trigger(QnActions::OpenInLayoutAction, QnActionParameters(medias).withArgument(Qn::LayoutResourceRole, layout));
    } else if(QnUserResourcePtr user = node->resource().dynamicCast<QnUserResource>()) {
        foreach(const QnResourcePtr &resource, resources) {
            if(resource->getParentId() == user->getId())
                continue; /* Dropping resource into its owner does nothing. */

            QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
            if(!layout)
                continue; /* Can drop only layout resources on user. */

            menu()->trigger(
                QnActions::SaveLayoutAsAction,
                QnActionParameters(layout).
                withArgument(Qn::UserResourceRole, user).
                withArgument(Qn::ResourceNameRole, layout->getName())
                );
        }


    } else if(QnMediaServerResourcePtr server = node->resource().dynamicCast<QnMediaServerResource>()) {
        if (QnMediaServerResource::isFakeServer(server))
            return true;

        if(mimeData->data(QLatin1String(pureTreeResourcesOnlyMimeType)) == QByteArray("1")) {
            /* Allow drop of non-layout item data, from tree only. */

            QnNetworkResourceList cameras = resources.filtered<QnNetworkResource>();
            if(!cameras.empty())
                menu()->trigger(QnActions::MoveCameraAction, QnActionParameters(cameras).withArgument(Qn::MediaServerResourceRole, server));
        }
    }

    return true;
}

Qt::DropActions QnResourceTreeModel::supportedDropActions() const {
    return Qt::CopyAction | Qt::MoveAction;
}


// -------------------------------------------------------------------------- //
// QnResourceTreeModel :: handlers
// -------------------------------------------------------------------------- //
void QnResourceTreeModel::at_resPool_resourceAdded(const QnResourcePtr &resource) {
    NX_ASSERT(resource);
    if (!resource)
        return;

    connect(resource,       &QnResource::parentIdChanged,                this,  &QnResourceTreeModel::at_resource_parentIdChanged);
    connect(resource,       &QnResource::nameChanged,                    this,  &QnResourceTreeModel::at_resource_resourceChanged);
    connect(resource,       &QnResource::statusChanged,                  this,  &QnResourceTreeModel::at_resource_resourceChanged);
    connect(resource,       &QnResource::urlChanged,                     this,  &QnResourceTreeModel::at_resource_resourceChanged);
    connect(resource,       &QnResource::flagsChanged,                   this,  &QnResourceTreeModel::at_resource_resourceChanged);

    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    if(layout) {
        connect(layout,     &QnLayoutResource::itemAdded,               this,   &QnResourceTreeModel::at_layout_itemAdded);
        connect(layout,     &QnLayoutResource::itemRemoved,             this,   &QnResourceTreeModel::at_layout_itemRemoved);
    }

    QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
    if (videoWall) {
        connect(videoWall,  &QnVideoWallResource::itemAdded,            this,   &QnResourceTreeModel::at_videoWall_itemAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::itemChanged,          this,   &QnResourceTreeModel::at_videoWall_itemAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::itemRemoved,          this,   &QnResourceTreeModel::at_videoWall_itemRemoved);

        connect(videoWall,  &QnVideoWallResource::matrixAdded,          this,   &QnResourceTreeModel::at_videoWall_matrixAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::matrixChanged,        this,   &QnResourceTreeModel::at_videoWall_matrixAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::matrixRemoved,        this,   &QnResourceTreeModel::at_videoWall_matrixRemoved);
    }

    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>()) {
        connect(camera,     &QnVirtualCameraResource::groupIdChanged,   this,   &QnResourceTreeModel::at_resource_parentIdChanged);
        connect(camera,     &QnVirtualCameraResource::groupNameChanged, this,   &QnResourceTreeModel::at_camera_groupNameChanged);
        connect(camera,     &QnVirtualCameraResource::statusFlagsChanged, this, &QnResourceTreeModel::at_resource_resourceChanged);
        auto updateParent = [this](const QnResourcePtr &resource) {
            /* Automatically update display name of the EDGE server if its camera was renamed. */
            QnResourcePtr parent = resource->getParentResource();
            if (QnMediaServerResource::isEdgeServer(parent))
                at_resource_resourceChanged(parent);
        };
        connect(camera, &QnResource::nameChanged, this, updateParent);
        updateParent(camera);
    }

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (server) {
        connect(server,     &QnMediaServerResource::redundancyChanged,  this,   &QnResourceTreeModel::at_server_redundancyChanged);

        if (QnMediaServerResource::isFakeServer(server))
            connect(server, &QnMediaServerResource::systemNameChanged,  this,   &QnResourceTreeModel::at_server_systemNameChanged);
    }

    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (user) {
        connect(user, &QnUserResource::enabledChanged, this, &QnResourceTreeModel::at_user_enabledChanged);
    }

    QnResourceTreeModelNode *node = this->node(resource);
    node->setResource(resource);

    at_resource_parentIdChanged(resource);
    at_resource_resourceChanged(resource);

    if (server) {
        m_rootNodes[Qn::OtherSystemsNode]->update();
        for (const QnResourcePtr &resource: qnResPool->getResourcesByParentId(server->getId())) {
            if (m_resourceNodeByResource.contains(resource))
                at_resource_parentIdChanged(resource);
        }
    }

    if(layout)
        foreach(const QnLayoutItemData &item, layout->getItems())
            at_layout_itemAdded(layout, item);

    if (videoWall) {
        foreach(const QnVideoWallItem &item, videoWall->items()->getItems())
            at_videoWall_itemAddedOrChanged(videoWall, item);
        foreach(const QnVideoWallMatrix &matrix, videoWall->matrices()->getItems())
            at_videoWall_matrixAddedOrChanged(videoWall, matrix);
    }

    if (QnWebPageResourcePtr webPage = resource.dynamicCast<QnWebPageResource>())
        m_rootNodes[Qn::WebPagesNode]->update();


}

void QnResourceTreeModel::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    if (!resource)
        return;

    disconnect(resource, NULL, this, NULL);

    QList<QPointer<QnResourceTreeModelNode> > nodesToDelete;
    for (QnResourceTreeModelNode* node: m_allNodes) {
        if (node->resource() == resource)
            nodesToDelete << QPointer<QnResourceTreeModelNode>(node);
    }

    for (auto node: nodesToDelete) {
        if (node.isNull())
            continue;
        removeNode(node.data());
    }

    m_itemNodesByResource.remove(resource);
    m_recorderHashByResource.remove(resource);
    NX_ASSERT(!m_resourceNodeByResource.contains(resource));

    if (QnWebPageResourcePtr webPage = resource.dynamicCast<QnWebPageResource>())
        m_rootNodes[Qn::WebPagesNode]->update();
}


void QnResourceTreeModel::rebuildTree() {
    m_rootNodes[Qn::LocalNode]->update();
    m_rootNodes[Qn::ServersNode]->update();
    m_rootNodes[Qn::UsersNode]->update();
    m_rootNodes[Qn::WebPagesNode]->update();
    m_rootNodes[Qn::OtherSystemsNode]->update();

    foreach(QnResourceTreeModelNode *node, m_resourceNodeByResource)
        node->setParent(expectedParent(node));
}

void QnResourceTreeModel::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource) {
    QnVideoWallResourcePtr videowall = resource->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    QnResourceTreeModelNode *node = videowall
        ? this->node(videowall)
        : this->node(resource);

    node->setModified(snapshotManager()->isModified(resource));
    node->update();
}

void QnResourceTreeModel::at_accessController_permissionsChanged(const QnResourcePtr &resource) {
    node(resource)->update();
    if (resource == context()->user())
        rebuildTree();
}

void QnResourceTreeModel::at_resource_parentIdChanged(const QnResourcePtr &resource) {
    QnResourceTreeModelNode *node = this->node(resource);

    /* update edge resource */
    if (resource.dynamicCast<QnVirtualCameraResource>()) {
        QnResourcePtr server = resource->getParentResource();
        bool wasEdge = (node->type() == Qn::EdgeNode);
        bool mustBeEdge = QnMediaServerResource::isHiddenServer(server);
        if (wasEdge != mustBeEdge) {
            QnResourceTreeModelNode *parent = node->parent();

            m_resourceNodeByResource.remove(resource);
            deleteNode(node);

            if (parent)
                parent->update();

            node = this->node(resource);

            if (QnResourceTreeModelNode *node = m_resourceNodeByResource.value(server))
                node->update();
        }
    }

    node->setParent(expectedParent(node));
}

void QnResourceTreeModel::at_resource_resourceChanged(const QnResourcePtr &resource) {
    QnResourceTreeModelNode *node = this->node(resource);

    node->update();

    if (m_itemNodesByResource.contains(resource))
        foreach(QnResourceTreeModelNode *node, m_itemNodesByResource[resource])
            node->update();
}

void QnResourceTreeModel::at_layout_itemAdded(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item) {
    QnResourceTreeModelNode *parentNode = this->node(layout);
    QnResourceTreeModelNode *node = this->node(item.uuid);

    QnResourcePtr resource;
    if(!item.resource.id.isNull()) { // TODO: #EC2
        resource = resourcePool()->getResourceById(item.resource.id);
    } else {
        resource = resourcePool()->getResourceByUniqueId(item.resource.path);
    }
    //NX_ASSERT(resource);   //too many strange situations with invalid resources in layout items

    node->setResource(resource);
    node->setParent(parentNode);
    node->update();
}

void QnResourceTreeModel::at_layout_itemRemoved(const QnLayoutResourcePtr &, const QnLayoutItemData &item) {
    removeNode(node(item.uuid));
}

void QnResourceTreeModel::at_videoWall_itemAddedOrChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    QnResourceTreeModelNode *parentNode = this->node(videoWall);

    QnResourceTreeModelNode *node = this->node(Qn::VideoWallItemNode, item.uuid, videoWall);

    QnResourcePtr resource;
    if(!item.layout.isNull())
        resource = resourcePool()->getResourceById(item.layout);

    node->setResource(resource);
    node->setParent(parentNode);
    node->update(); // in case of _changed method call
}

void QnResourceTreeModel::at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    Q_UNUSED(videoWall)
    removeNode(Qn::VideoWallItemNode, item.uuid, videoWall);
}

void QnResourceTreeModel::at_videoWall_matrixAddedOrChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallMatrix &matrix) {
    QnResourceTreeModelNode *parentNode = this->node(videoWall);

    QnResourceTreeModelNode *node = this->node(Qn::VideoWallMatrixNode, matrix.uuid, videoWall);

    node->setParent(parentNode);
    node->update(); // in case of _changed method call
}

void QnResourceTreeModel::at_videoWall_matrixRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallMatrix &matrix) {
    Q_UNUSED(videoWall)
    removeNode(Qn::VideoWallMatrixNode, matrix.uuid, videoWall);
}

void QnResourceTreeModel::at_camera_groupNameChanged(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    NX_ASSERT(camera);
    if (!camera)
        return;

    const QString groupId = camera->getGroupId();
    foreach (RecorderHash recorderHash, m_recorderHashByResource) {
        if (!recorderHash.contains(groupId))
            continue;
        QnResourceTreeModelNode* recorder = recorderHash[groupId];
        recorder->m_name = camera->getGroupName();
        recorder->m_displayName = camera->getGroupName();
        recorder->changeInternal();
    }
}

void QnResourceTreeModel::at_server_systemNameChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    NX_ASSERT(server);
    if (!server)
        return;

    QnResourceTreeModelNode *node = this->node(resource);
    node->setParent(expectedParent(node));
    m_rootNodes[Qn::OtherSystemsNode]->update();
}

void QnResourceTreeModel::at_server_redundancyChanged(const QnResourcePtr &resource) {
    QnResourceTreeModelNode *node = this->node(resource);
    node->update();

    for (const QnVirtualCameraResourcePtr &cameraResource: qnResPool->getAllCameras(resource, true)) {
        QnResourceTreeModelNode *camNode = m_resourceNodeByResource.take(cameraResource);
        deleteNode(camNode);
        /* Re-create node as it should change its NodeType from ResourceNode to EdgeNode or vice versa. */
        camNode = this->node(cameraResource);
        camNode->setParent(expectedParent(camNode));
    }
}

void QnResourceTreeModel::at_commonModule_systemNameChanged() {
    m_rootNodes[Qn::ServersNode]->update();
}

void QnResourceTreeModel::at_user_enabledChanged(const QnResourcePtr &resource) {
    QnResourceTreeModelNode *node = this->node(resource);
    node->setParent(expectedParent(node));
}

void QnResourceTreeModel::at_serverAutoDiscoveryEnabledChanged() {
    m_rootNodes[Qn::OtherSystemsNode]->update();
}
