#include "resource_pool_model.h"
#include <cassert>

#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>

#include <utils/common/checked_cast.h>
#include <common/common_meta_types.h>

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

} // namespace

// -------------------------------------------------------------------------- //
// QnResourcePoolModel :: contructors, destructor and helpers.
// -------------------------------------------------------------------------- //
QnResourcePoolModel::QnResourcePoolModel(Qn::NodeType rootNodeType, QObject *parent):
    base_type(parent), 
    QnWorkbenchContextAware(parent),
    m_urlsShown(true),
    m_rootNodeType(rootNodeType)
{
    m_rootNodeTypes << Qn::LocalNode << Qn::UsersNode << Qn::ServersNode << Qn::RootNode << Qn::BastardNode;

    /* Create top-level nodes. */
    foreach(Qn::NodeType t, m_rootNodeTypes)
        m_rootNodes[t] = new QnResourcePoolModelNode(this, t);

    Qn::NodeType parentNodeType = rootNodeType == Qn::RootNode ? Qn::RootNode : Qn::BastardNode;
    foreach(Qn::NodeType t, m_rootNodeTypes)
        if (t != rootNodeType && t != parentNodeType)
            m_rootNodes[t]->setParent(m_rootNodes[parentNodeType]);

    /* Connect to context. */
    connect(resourcePool(),     &QnResourcePool::resourceAdded,                     this,   &QnResourcePoolModel::at_resPool_resourceAdded, Qt::QueuedConnection);
    connect(resourcePool(),     &QnResourcePool::resourceRemoved,                   this,   &QnResourcePoolModel::at_resPool_resourceRemoved, Qt::QueuedConnection);
    connect(snapshotManager(),  &QnWorkbenchLayoutSnapshotManager::flagsChanged,    this,   &QnResourcePoolModel::at_snapshotManager_flagsChanged);
    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged,   this,   &QnResourcePoolModel::at_accessController_permissionsChanged);
    connect(context(),          &QnWorkbenchContext::userChanged,                   this,   &QnResourcePoolModel::at_context_userChanged, Qt::QueuedConnection);

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

    foreach(const QnResourcePtr &resource, resources)
        at_resPool_resourceRemoved(resource);

    /* Free memory. */
    qDeleteAll(m_resourceNodeByResource);
    qDeleteAll(m_itemNodeByUuid);
    foreach(Qn::NodeType t, m_rootNodeTypes) {
        delete m_rootNodes[t];
        qDeleteAll(m_nodes[t]);
    }
}

QnResourcePtr QnResourcePoolModel::resource(const QModelIndex &index) const {
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

QnResourcePoolModelNode *QnResourcePoolModel::node(const QnResourcePtr &resource) {
    if(!resource)
        return m_rootNodes[m_rootNodeType];

    QHash<QnResourcePtr, QnResourcePoolModelNode *>::iterator pos = m_resourceNodeByResource.find(resource);
    if(pos == m_resourceNodeByResource.end()) {
        Qn::NodeType nodeType = QnMediaServerResource::isEdgeServer(resource->getParentResource())
            ? Qn::EdgeNode
            : Qn::ResourceNode;
        pos = m_resourceNodeByResource.insert(resource, new QnResourcePoolModelNode(this, resource, nodeType));

    }
    return *pos;
}

QnResourcePoolModelNode *QnResourcePoolModel::node(Qn::NodeType nodeType, const QUuid &uuid, const QnResourcePtr &resource) {
    NodeKey key(resource, uuid);
    if (!m_nodes[nodeType].contains(key)) {
        QnResourcePoolModelNode* node = new QnResourcePoolModelNode(this, uuid, nodeType);
        m_nodes[nodeType].insert(key, node);
        return node;
    }
    return m_nodes[nodeType][key];
}

QnResourcePoolModelNode *QnResourcePoolModel::node(const QUuid &uuid) {
    QHash<QUuid, QnResourcePoolModelNode *>::iterator pos = m_itemNodeByUuid.find(uuid);
    if(pos == m_itemNodeByUuid.end())
        pos = m_itemNodeByUuid.insert(uuid, new QnResourcePoolModelNode(this, uuid));
    return *pos;
}

QnResourcePoolModelNode *QnResourcePoolModel::node(const QModelIndex &index) const {
    if(!index.isValid())
        return m_rootNodes[m_rootNodeType];

    return static_cast<QnResourcePoolModelNode *>(index.internalPointer());
}

QnResourcePoolModelNode *QnResourcePoolModel::node(const QnResourcePtr &resource, const QString &groupId, const QString &groupName) {
    RecorderHash recorderHash = m_recorderHashByResource[resource];
    if (recorderHash.contains(groupId))
        return recorderHash[groupId];

    QnResourcePoolModelNode* recorder = new QnResourcePoolModelNode(this, Qn::RecorderNode, groupName);
    recorder->setParent(m_resourceNodeByResource[resource]);
    recorderHash[groupId] = recorder;
    return recorder;
}

void QnResourcePoolModel::deleteNode(QnResourcePoolModelNode *node) {
    switch(node->type()) {
    case Qn::VideoWallItemNode:
    case Qn::VideoWallMatrixNode:
        deleteNode(node->type(), node->uuid(), node->resource());
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
    default:
        assert(false); //should never come here
    }

    delete node;
}

void QnResourcePoolModel::deleteNode(Qn::NodeType nodeType, const QUuid &uuid, const QnResourcePtr &resource) {
    NodeKey key(resource, uuid);
    if (!m_nodes[nodeType].contains(key))
        return;

    delete m_nodes[nodeType].take(key);
}

QnResourcePoolModelNode *QnResourcePoolModel::expectedParent(QnResourcePoolModelNode *node) {
    assert(node->type() == Qn::ResourceNode || node->type() == Qn::EdgeNode);

    if(!node->resource())
        return m_rootNodes[m_rootNodeType];

    if(node->resourceFlags() & QnResource::user) {
        if(!accessController()->hasGlobalPermissions(Qn::GlobalEditUsersPermission)) {
            return m_rootNodes[m_rootNodeType];
        } else {
            return m_rootNodes[Qn::UsersNode];
        }
    }

    if (node->type() == Qn::EdgeNode)
        return m_rootNodes[Qn::ServersNode];

    if(node->resourceFlags() & QnResource::server)
        return m_rootNodes[Qn::ServersNode];

    if (node->resourceFlags() & QnResource::videowall)
        return m_rootNodes[m_rootNodeType];

    // We are requesting for the list of users so we don't want to see their layouts
    if (m_rootNodeType == Qn::UsersNode && !(node->resourceFlags() &  QnResource::user)) 
        return m_rootNodes[Qn::BastardNode];

    QnResourcePtr parentResource = resourcePool()->getResourceById(node->resource()->getParentId());
    if(!parentResource || (parentResource->flags() & QnResource::local_server) == QnResource::local_server) {
        if(node->resourceFlags() & QnResource::local) {
            return m_rootNodes[Qn::LocalNode];
        } else {
            return NULL;
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

    m_rootNodes[m_rootNodeType]->updateRecursive();
}

Qn::NodeType QnResourcePoolModel::rootNodeType() const {
    return m_rootNodeType;
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
    QnResourcePoolModelNode* n = node(parent);
    return n->children().size();
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
            QSet<QUuid> uuids;
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
    qDebug() << "Qt::DropAction" << action;

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

    if(node->parent() && (node->parent()->resourceFlags() & QnResource::server))
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
    connect(resource,       &QnResource::resourceChanged,                this,  &QnResourcePoolModel::at_resource_resourceChanged);

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
        connect(camera,     &QnVirtualCameraResource::groupNameChanged, this,   &QnResourcePoolModel::at_camera_groupNameChanged);
    }

    QnResourcePoolModelNode *node = this->node(resource);
    node->setResource(resource);

    at_resource_parentIdChanged(resource);
    at_resource_resourceChanged(resource);

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

    delete m_resourceNodeByResource.take(resource);
}

void QnResourcePoolModel::at_context_userChanged() {
    m_rootNodes[Qn::LocalNode]->update();
    m_rootNodes[Qn::ServersNode]->update();
    m_rootNodes[Qn::UsersNode]->update();

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

    node->setParent(expectedParent(node));
}

void QnResourcePoolModel::at_resource_resourceChanged(const QnResourcePtr &resource) {
    node(resource)->update();

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

    node->setResource(resource);
    node->setParent(parentNode);
}

void QnResourcePoolModel::at_layout_itemRemoved(const QnLayoutResourcePtr &, const QnLayoutItemData &item) {
    deleteNode(node(item.uuid));
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
    deleteNode(Qn::VideoWallItemNode, item.uuid, videoWall);
}

void QnResourcePoolModel::at_videoWall_matrixAddedOrChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallMatrix &matrix) {
    QnResourcePoolModelNode *parentNode = this->node(videoWall);

    QnResourcePoolModelNode *node = this->node(Qn::VideoWallMatrixNode, matrix.uuid, videoWall);

    node->setParent(parentNode);
    node->update(); // in case of _changed method call
}

void QnResourcePoolModel::at_videoWall_matrixRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallMatrix &matrix) {
    Q_UNUSED(videoWall)
    deleteNode(Qn::VideoWallMatrixNode, matrix.uuid, videoWall);
}

void QnResourcePoolModel::at_camera_groupNameChanged(const QnSecurityCamResourcePtr &camera) {
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

void QnResourcePoolModel::setToFlatMode( bool flat ) {
    QnResourcePoolModelNode* root_node = m_rootNodes[m_rootNodeType];
    Q_ASSERT(root_node);

    // The root_node here is root of the tree, what we really need to hide
    // is the root of the tree's grand children.If we hide the children of
    // that tree root, we gonna end up with nothing to show here. 

    foreach(QnResourcePoolModelNode* child,root_node->children()) {
        if( child != NULL ) {
            foreach(QnResourcePoolModelNode* grand_child,child->children() ) {
                grand_child->setBastard(flat);
            }
        }
    }
}