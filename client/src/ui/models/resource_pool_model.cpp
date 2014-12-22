#include "resource_pool_model.h"
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
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_matrix.h>

#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_pc_data.h>

#include <core/resource_management/resource_pool.h>
#include <ui/actions/action_manager.h>
#include <ui/common/ui_resource_name.h>
#include <ui/models/resource_pool_model_node.h>
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

    Qn::NodeType rootNodeTypeForScope(QnResourcePoolModel::Scope scope) {
        switch (scope) {
        case QnResourcePoolModel::CamerasScope:
            return Qn::ServersNode;
        case QnResourcePoolModel::UsersScope:
            return Qn::UsersNode;
        default:
            return Qn::RootNode;
        }
    }

} // namespace

// -------------------------------------------------------------------------- //
// QnResourcePoolModel :: contructors, destructor and helpers.
// -------------------------------------------------------------------------- //
QnResourcePoolModel::QnResourcePoolModel(Scope scope, QObject *parent):
    base_type(parent), 
    QnWorkbenchContextAware(parent),
    m_urlsShown(true),
    m_scope(scope)
{
    m_rootNodeTypes << Qn::LocalNode << Qn::UsersNode << Qn::ServersNode << Qn::OtherSystemsNode << Qn::RootNode << Qn::BastardNode;

    /* Create top-level nodes. */
    foreach(Qn::NodeType t, m_rootNodeTypes) {
        m_rootNodes[t] = new QnResourcePoolModelNode(this, t);
        m_allNodes.append(m_rootNodes[t]);
    }

    
    Qn::NodeType parentNodeType = scope == FullScope ? Qn::RootNode : Qn::BastardNode;
    Qn::NodeType rootNodeType = rootNodeTypeForScope(m_scope);

    foreach(Qn::NodeType t, m_rootNodeTypes)
        if (t != rootNodeType && t != parentNodeType)
            m_rootNodes[t]->setParent(m_rootNodes[parentNodeType]);

    /* Connect to context. */
    connect(resourcePool(),     &QnResourcePool::resourceAdded,                     this,   &QnResourcePoolModel::at_resPool_resourceAdded, Qt::QueuedConnection);
    connect(resourcePool(),     &QnResourcePool::resourceRemoved,                   this,   &QnResourcePoolModel::at_resPool_resourceRemoved, Qt::QueuedConnection);
    connect(snapshotManager(),  &QnWorkbenchLayoutSnapshotManager::flagsChanged,    this,   &QnResourcePoolModel::at_snapshotManager_flagsChanged);
    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged,   this,   &QnResourcePoolModel::at_accessController_permissionsChanged);
    connect(context(),          &QnWorkbenchContext::userChanged,                   this,   &QnResourcePoolModel::at_context_userChanged, Qt::QueuedConnection);
    connect(qnCommon,           &QnCommonModule::systemNameChanged,                 this,   &QnResourcePoolModel::at_commonModule_systemNameChanged);

    QnResourceList resources = resourcePool()->getResources(); 

    at_context_userChanged();

    /* It is important to connect before iterating as new resources may be added to the pool asynchronously. */
    foreach(const QnResourcePtr &resource, resources)
        at_resPool_resourceAdded(resource);

}

QnResourcePoolModel::~QnResourcePoolModel() {
    /* Disconnect from context. */
    QnResourceList resources = resourcePool()->getResources(); 
    disconnect(resourcePool(), NULL, this, NULL);
    disconnect(snapshotManager(), NULL, this, NULL);

    /* Make sure all nodes will have their parent empty. */
    foreach(QnResourcePoolModelNode* node, m_allNodes)
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

QnResourcePtr QnResourcePoolModel::resource(const QModelIndex &index) const {
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

QnResourcePoolModelNode *QnResourcePoolModel::node(const QnResourcePtr &resource) {
    if(!resource)
        return m_rootNodes[Qn::BastardNode];

    QHash<QnResourcePtr, QnResourcePoolModelNode *>::iterator pos = m_resourceNodeByResource.find(resource);
    if(pos == m_resourceNodeByResource.end()) {
        Qn::NodeType nodeType = Qn::ResourceNode;
        if (QnMediaServerResource::isHiddenServer(resource->getParentResource()))
            nodeType = Qn::EdgeNode;

        pos = m_resourceNodeByResource.insert(resource, new QnResourcePoolModelNode(this, resource, nodeType));
        m_allNodes.append(*pos);
    }
    return *pos;
}

QnResourcePoolModelNode *QnResourcePoolModel::node(Qn::NodeType nodeType, const QnUuid &uuid, const QnResourcePtr &resource) {
    NodeKey key(resource, uuid);
    if (!m_nodes[nodeType].contains(key)) {
        QnResourcePoolModelNode* node = new QnResourcePoolModelNode(this, uuid, nodeType);
        m_nodes[nodeType].insert(key, node);
        m_allNodes.append(node);
        return node;
    }
    return m_nodes[nodeType][key];
}

QnResourcePoolModelNode *QnResourcePoolModel::node(const QnUuid &uuid) {
    QHash<QnUuid, QnResourcePoolModelNode *>::iterator pos = m_itemNodeByUuid.find(uuid);
    if(pos == m_itemNodeByUuid.end()) {
        pos = m_itemNodeByUuid.insert(uuid, new QnResourcePoolModelNode(this, uuid));
        m_allNodes.append(*pos);
    }
    return *pos;
}

QnResourcePoolModelNode *QnResourcePoolModel::node(const QModelIndex &index) const {
    /* Root node. */
    if(!index.isValid())
        return m_rootNodes[rootNodeTypeForScope(m_scope)];

    return static_cast<QnResourcePoolModelNode *>(index.internalPointer());
}

QnResourcePoolModelNode *QnResourcePoolModel::node(const QnResourcePtr &resource, const QString &groupId, const QString &groupName) {
    if (m_recorderHashByResource[resource].contains(groupId))
        return m_recorderHashByResource[resource][groupId];

    QnResourcePoolModelNode* recorder = new QnResourcePoolModelNode(this, Qn::RecorderNode, groupName);
    if (m_scope != UsersScope)
        recorder->setParent(m_resourceNodeByResource[resource]);
    else
        recorder->setParent(m_rootNodes[Qn::BastardNode]);
    m_recorderHashByResource[resource][groupId] = recorder;
    m_allNodes.append(recorder);
    return recorder;
}

QnResourcePoolModelNode *QnResourcePoolModel::systemNode(const QString &systemName) {
    QnResourcePoolModelNode *node = m_systemNodeBySystemName[systemName];
    if (node)
        return node;

    node = new QnResourcePoolModelNode(this, Qn::SystemNode, systemName);
    if (m_scope == FullScope)
        node->setParent(m_rootNodes[Qn::OtherSystemsNode]);
    else
        node->setParent(m_rootNodes[Qn::BastardNode]);
    m_systemNodeBySystemName[systemName] = node;
    m_allNodes.append(node);
    return node;
}

void QnResourcePoolModel::removeNode(QnResourcePoolModelNode *node) {
    switch(node->type()) {
    case Qn::VideoWallItemNode:
    case Qn::VideoWallMatrixNode:
        removeNode(node->type(), node->uuid(), node->resource());
        return;

    case Qn::ResourceNode:
        m_resourceNodeByResource.remove(node->resource());
        break;
    case Qn::ItemNode:
        m_itemNodeByUuid.remove(node->uuid());
        if (node->resource())
            m_itemNodesByResource[node->resource()].removeAll(node);
        break;
    case Qn::RecorderNode:
        break;  //nothing special
    case Qn::SystemNode:
        break;  //nothing special
    default:
        assert(false); //should never come here
    }

    deleteNode(node);
}

void QnResourcePoolModel::removeNode(Qn::NodeType nodeType, const QnUuid &uuid, const QnResourcePtr &resource) {
    NodeKey key(resource, uuid);
    if (!m_nodes[nodeType].contains(key))
        return;

    deleteNode(m_nodes[nodeType].take(key));
}

void QnResourcePoolModel::deleteNode(QnResourcePoolModelNode *node) {
    Q_ASSERT(m_allNodes.contains(node));
    m_allNodes.removeOne(node);
    foreach (QnResourcePoolModelNode* existing, m_allNodes)
        if (existing->parent() == node)
            existing->setParent(NULL);

    delete node;
}

QnResourcePoolModelNode *QnResourcePoolModel::expectedParent(QnResourcePoolModelNode *node) {
    assert(node->type() == Qn::ResourceNode || node->type() == Qn::EdgeNode);

    if(!node->resource())
        return m_rootNodes[Qn::BastardNode];

    if(node->resourceFlags() & Qn::user) {
        if (m_scope == UsersScope)
            return m_rootNodes[Qn::UsersNode];
        if (m_scope == CamerasScope)
            return m_rootNodes[Qn::BastardNode];

        //TODO: #GDM non-admin scope?
        if(!accessController()->hasGlobalPermissions(Qn::GlobalEditUsersPermission)) {
            return m_rootNodes[Qn::RootNode];
        } else {
            return m_rootNodes[Qn::UsersNode];
        }
    }

    /* In UsersScope all other nodes should be hidden. */
    if (m_scope == UsersScope)
        return m_rootNodes[Qn::BastardNode];


    if (node->type() == Qn::EdgeNode)
        return m_rootNodes[Qn::ServersNode];

    if (node->resourceFlags() & Qn::server) {
        if (node->resourceStatus() == Qn::Incompatible) {
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

    QnResourcePtr parentResource = resourcePool()->getResourceById(node->resource()->getParentId());
    if(!parentResource || (parentResource->flags() & Qn::local_server) == Qn::local_server) {
        if(node->resourceFlags() & Qn::local) {
            return m_rootNodes[Qn::LocalNode];
        } else {
            return m_rootNodes[Qn::BastardNode];
        }
    } else {
        QnResourcePoolModelNode* parent = this->node(parentResource);

        if (QnSecurityCamResourcePtr camera = node->resource().dynamicCast<QnSecurityCamResource>()) {
            QString groupName = camera->getGroupName();
            QString groupId = camera->getGroupId();
            if(!groupId.isEmpty())
                parent = this->node(parentResource, groupId, groupName.isEmpty() ? groupId : groupName);
        }

        return parent;
    }
}

bool QnResourcePoolModel::isUrlsShown() {
    return m_urlsShown;
}

void QnResourcePoolModel::setUrlsShown(bool urlsShown) {
    if(urlsShown == m_urlsShown)
        return;

    m_urlsShown = urlsShown;

    Qn::NodeType rootNodeType = rootNodeTypeForScope(m_scope);
    m_rootNodes[rootNodeType]->updateRecursive();
}

// -------------------------------------------------------------------------- //
// QnResourcePoolModel :: QAbstractItemModel implementation
// -------------------------------------------------------------------------- //
QModelIndex QnResourcePoolModel::index(int row, int column, const QModelIndex &parent) const {
    if(!hasIndex(row, column, parent)) /* hasIndex calls rowCount and columnCount. */
        return QModelIndex();

    return node(parent)->child(row)->index(row, column);
}

QModelIndex QnResourcePoolModel::buddy(const QModelIndex &index) const {
    return index;
}

QModelIndex QnResourcePoolModel::parent(const QModelIndex &index) const {
    if (!index.isValid() || index.model() != this)
        return QModelIndex();
    return node(index)->parent()->index(Qn::NameColumn);
}

bool QnResourcePoolModel::hasChildren(const QModelIndex &parent) const {
    return rowCount(parent) > 0;
}

int QnResourcePoolModel::rowCount(const QModelIndex &parent) const {
    if (parent.column() > Qn::NameColumn)
        return 0;

    return node(parent)->children().size();
}

int QnResourcePoolModel::columnCount(const QModelIndex &/*parent*/) const {
    return Qn::ColumnCount;
}

Qt::ItemFlags QnResourcePoolModel::flags(const QModelIndex &index) const {
    if(!index.isValid())
        return Qt::NoItemFlags;
    return node(index)->flags(index.column());
}

QVariant QnResourcePoolModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    return node(index)->data(role, index.column());
}

bool QnResourcePoolModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if(!index.isValid())
        return false;

    return node(index)->setData(value, role, index.column());
}

QVariant QnResourcePoolModel::headerData(int section, Qt::Orientation orientation, int role) const {
    Q_UNUSED(section);
    Q_UNUSED(orientation);
    Q_UNUSED(role);

    return QVariant(); /* No headers needed. */
}

QHash<int,QByteArray> QnResourcePoolModel::roleNames() const {
    QHash<int, QByteArray> roles = base_type::roleNames();
    roles.insert(Qn::ResourceRole,              "resource");
    roles.insert(Qn::ResourceFlagsRole,         "flags");
    roles.insert(Qn::ItemUuidRole,              "uuid");
    roles.insert(Qn::ResourceSearchStringRole,  "searchString");
    roles.insert(Qn::ResourceStatusRole,        "status");
    roles.insert(Qn::NodeTypeRole,              "nodeType");
    return roles;
}

QStringList QnResourcePoolModel::mimeTypes() const {
    QStringList result = QnWorkbenchResource::resourceMimeTypes();
    result.append(QLatin1String(pureTreeResourcesOnlyMimeType));
    result.append(QnVideoWallItem::mimeType());
    return result;
}

QMimeData *QnResourcePoolModel::mimeData(const QModelIndexList &indexes) const {
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
                QnResourcePoolModelNode *node = this->node(index);

                if (node && node->type() == Qn::RecorderNode) {
                    foreach (QnResourcePoolModelNode* child, node->children()) {
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
                QnResourcePoolModelNode *node = this->node(index);

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

bool QnResourcePoolModel::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
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
    QnResourcePoolModelNode *node = this->node(parent);

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
        menu()->trigger(Qn::DropOnVideoWallItemAction, parameters);
    } else if(QnLayoutResourcePtr layout = node->resource().dynamicCast<QnLayoutResource>()) {
        QnResourceList medias;
        foreach( QnResourcePtr res, resources )
        {
            if( res.dynamicCast<QnMediaResource>() )
                medias.push_back( res );
        }

        menu()->trigger(Qn::OpenInLayoutAction, QnActionParameters(medias).withArgument(Qn::LayoutResourceRole, layout));
    } else if(QnUserResourcePtr user = node->resource().dynamicCast<QnUserResource>()) {
        foreach(const QnResourcePtr &resource, resources) {
            if(resource->getParentId() == user->getId())
                continue; /* Dropping resource into its owner does nothing. */

            QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
            if(!layout)
                continue; /* Can drop only layout resources on user. */

            menu()->trigger(
                Qn::SaveLayoutAsAction, 
                QnActionParameters(layout).
                withArgument(Qn::UserResourceRole, user).
                withArgument(Qn::ResourceNameRole, layout->getName())
                );
        }


    } else if(QnMediaServerResourcePtr server = node->resource().dynamicCast<QnMediaServerResource>()) {
        if(mimeData->data(QLatin1String(pureTreeResourcesOnlyMimeType)) == QByteArray("1")) {
            /* Allow drop of non-layout item data, from tree only. */

            QnNetworkResourceList cameras = resources.filtered<QnNetworkResource>();
            if(!cameras.empty())
                menu()->trigger(Qn::MoveCameraAction, QnActionParameters(cameras).withArgument(Qn::MediaServerResourceRole, server));
        }
    }

    return true;
}

Qt::DropActions QnResourcePoolModel::supportedDropActions() const {
    return Qt::CopyAction | Qt::MoveAction;
}


// -------------------------------------------------------------------------- //
// QnResourcePoolModel :: handlers
// -------------------------------------------------------------------------- //
void QnResourcePoolModel::at_resPool_resourceAdded(const QnResourcePtr &resource) {
    assert(resource);

    connect(resource,       &QnResource::parentIdChanged,                this,  &QnResourcePoolModel::at_resource_parentIdChanged);
    connect(resource,       &QnResource::nameChanged,                    this,  &QnResourcePoolModel::at_resource_resourceChanged);
    connect(resource,       &QnResource::statusChanged,                  this,  &QnResourcePoolModel::at_resource_resourceChanged);
    connect(resource,       &QnResource::urlChanged,                     this,  &QnResourcePoolModel::at_resource_resourceChanged);
    connect(resource,       &QnResource::flagsChanged,                   this,  &QnResourcePoolModel::at_resource_resourceChanged);

    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    if(layout) {
        connect(layout,     &QnLayoutResource::itemAdded,               this,   &QnResourcePoolModel::at_layout_itemAdded);
        connect(layout,     &QnLayoutResource::itemRemoved,             this,   &QnResourcePoolModel::at_layout_itemRemoved);
    }

    QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
    if (videoWall) {
        connect(videoWall,  &QnVideoWallResource::itemAdded,            this,   &QnResourcePoolModel::at_videoWall_itemAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::itemChanged,          this,   &QnResourcePoolModel::at_videoWall_itemAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::itemRemoved,          this,   &QnResourcePoolModel::at_videoWall_itemRemoved);

        connect(videoWall,  &QnVideoWallResource::matrixAdded,          this,   &QnResourcePoolModel::at_videoWall_matrixAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::matrixChanged,        this,   &QnResourcePoolModel::at_videoWall_matrixAddedOrChanged);
        connect(videoWall,  &QnVideoWallResource::matrixRemoved,        this,   &QnResourcePoolModel::at_videoWall_matrixRemoved);
    }

    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>()) {
        connect(camera,     &QnVirtualCameraResource::groupIdChanged,   this,   &QnResourcePoolModel::at_resource_parentIdChanged);
        connect(camera,     &QnVirtualCameraResource::groupNameChanged, this,   &QnResourcePoolModel::at_camera_groupNameChanged);
        connect(camera,     &QnResource::nameChanged,                   this,   [this](const QnResourcePtr &resource) {
            /* Automatically update display name of the EDGE server if its camera was renamed. */
            QnResourcePtr parent = resource->getParentResource();
            if (QnMediaServerResource::isEdgeServer(parent))
                at_resource_resourceChanged(parent);
        });
    }


    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (server) {
        connect(server,     &QnMediaServerResource::redundancyChanged,  this,   &QnResourcePoolModel::at_server_redundancyChanged);

        if (server->getStatus() == Qn::Incompatible)
            connect(server, &QnMediaServerResource::systemNameChanged,  this,   &QnResourcePoolModel::at_server_systemNameChanged);
    }

    QnResourcePoolModelNode *node = this->node(resource);
    node->setResource(resource);

    at_resource_parentIdChanged(resource);
    at_resource_resourceChanged(resource);

    if (server) {
        m_rootNodes[Qn::OtherSystemsNode]->update();

        QnUuid serverId = server->getId();
        foreach (QnResourcePoolModelNode *node, m_rootNodes[Qn::BastardNode]->children()) {
            QnResourcePtr resource = node->resource();
            if (resource && resource->getParentId() == serverId)
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
}

void QnResourcePoolModel::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    disconnect(resource, NULL, this, NULL);

    if (!resource)
        return;

    if (!m_resourceNodeByResource.contains(resource))
        return;

    QnResourcePoolModelNode *node = m_resourceNodeByResource.take(resource);
    QnResourcePoolModelNode *parent = node->parent();

    deleteNode(node);

    if (parent)
        parent->update();
}


void QnResourcePoolModel::at_context_userChanged() {
    m_rootNodes[Qn::LocalNode]->update();
    m_rootNodes[Qn::ServersNode]->update();
    m_rootNodes[Qn::UsersNode]->update();
    m_rootNodes[Qn::OtherSystemsNode]->update();

    foreach(QnResourcePoolModelNode *node, m_resourceNodeByResource)
        node->setParent(expectedParent(node));
}

void QnResourcePoolModel::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource) {
    QnVideoWallResourcePtr videowall = resource->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    QnResourcePoolModelNode *node = videowall 
        ? this->node(videowall)
        : this->node(resource);

    node->setModified(snapshotManager()->isModified(resource));
    node->update();
}

void QnResourcePoolModel::at_accessController_permissionsChanged(const QnResourcePtr &resource) {
    node(resource)->update();
}

void QnResourcePoolModel::at_resource_parentIdChanged(const QnResourcePtr &resource) {
    QnResourcePoolModelNode *node = this->node(resource);

    /* update edge resource */
    if (resource.dynamicCast<QnVirtualCameraResource>()) {
        bool wasEdge = (node->type() == Qn::EdgeNode);
        bool mustBeEdge = QnMediaServerResource::isHiddenServer(resource->getParentResource());
        if (wasEdge != mustBeEdge) {
            QnResourcePoolModelNode *parent = node->parent();

            m_resourceNodeByResource.remove(resource);
            deleteNode(node);

            if (parent)
                parent->update();

            node = this->node(resource);
        }
    }

    node->setParent(expectedParent(node));
}

void QnResourcePoolModel::at_resource_resourceChanged(const QnResourcePtr &resource) {
    QnResourcePoolModelNode *node = this->node(resource);

    node->update();

    foreach(QnResourcePoolModelNode *node, m_itemNodesByResource[resource])
        node->update();
}

void QnResourcePoolModel::at_layout_itemAdded(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item) {
    QnResourcePoolModelNode *parentNode = this->node(layout);
    QnResourcePoolModelNode *node = this->node(item.uuid);

    QnResourcePtr resource;
    if(!item.resource.id.isNull()) { // TODO: #EC2
        resource = resourcePool()->getResourceById(item.resource.id);
    } else {
        resource = resourcePool()->getResourceByUniqId(item.resource.path);
    }
    //Q_ASSERT(resource);   //too many strange situations with invalid resources in layout items

    node->setResource(resource);
    node->setParent(parentNode);
}

void QnResourcePoolModel::at_layout_itemRemoved(const QnLayoutResourcePtr &, const QnLayoutItemData &item) {
    removeNode(node(item.uuid));
}

void QnResourcePoolModel::at_videoWall_itemAddedOrChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    QnResourcePoolModelNode *parentNode = this->node(videoWall);

    QnResourcePoolModelNode *node = this->node(Qn::VideoWallItemNode, item.uuid, videoWall);

    QnResourcePtr resource;
    if(!item.layout.isNull())
        resource = resourcePool()->getResourceById(item.layout);

    node->setResource(resource);
    node->setParent(parentNode);
    node->update(); // in case of _changed method call
}

void QnResourcePoolModel::at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    Q_UNUSED(videoWall)
    removeNode(Qn::VideoWallItemNode, item.uuid, videoWall);
}

void QnResourcePoolModel::at_videoWall_matrixAddedOrChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallMatrix &matrix) {
    QnResourcePoolModelNode *parentNode = this->node(videoWall);

    QnResourcePoolModelNode *node = this->node(Qn::VideoWallMatrixNode, matrix.uuid, videoWall);

    node->setParent(parentNode);
    node->update(); // in case of _changed method call
}

void QnResourcePoolModel::at_videoWall_matrixRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallMatrix &matrix) {
    Q_UNUSED(videoWall)
    removeNode(Qn::VideoWallMatrixNode, matrix.uuid, videoWall);
}

void QnResourcePoolModel::at_camera_groupNameChanged(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    Q_ASSERT(camera);
    if (!camera)
        return;

    const QString groupId = camera->getGroupId();
    foreach (RecorderHash recorderHash, m_recorderHashByResource) {
        if (!recorderHash.contains(groupId))
            continue;
        QnResourcePoolModelNode* recorder = recorderHash[groupId];
        recorder->m_name = camera->getGroupName();
        recorder->m_displayName = camera->getGroupName();
        recorder->changeInternal();
    }
}

void QnResourcePoolModel::at_server_systemNameChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    Q_ASSERT(server);
    if (!server)
        return;

    QnResourcePoolModelNode *node = this->node(resource);
    node->setParent(expectedParent(node));
    m_rootNodes[Qn::OtherSystemsNode]->update();
}

void QnResourcePoolModel::at_server_redundancyChanged(const QnResourcePtr &resource) {
    QnResourcePoolModelNode *node = this->node(resource);
    bool isHidden = QnMediaServerResource::isHiddenServer(resource);
    QnResourcePoolModelNode *camerasParentNode = isHidden ? m_rootNodes[Qn::ServersNode] : node;

    foreach (const QnResourcePtr &cameraResource, qnResPool->getAllCameras(resource, true)) {
        if (!cameraResource.dynamicCast<QnVirtualCameraResource>())
            continue;

        this->node(cameraResource)->setParent(camerasParentNode);
    }

    node->update();
}

void QnResourcePoolModel::at_commonModule_systemNameChanged() {
    m_rootNodes[Qn::ServersNode]->update();
}
