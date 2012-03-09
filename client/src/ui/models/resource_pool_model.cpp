#include "resource_pool_model.h"
#include <cassert>
#include <QMimeData>
#include <QUrl>
#include <utils/common/checked_cast.h>
#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <ui/actions/action_manager.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_resource.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include "file_processor.h"

namespace {
    enum Columns {
        NameColumn,
        ColumnCount
    };

    const char *pureTreeResourcesOnlyMimeType = "application/x-noptix-pure-tree-resources-only";

    bool intersects(const QStringList &l, const QStringList &r) {
        foreach(const QString &s, l)
            if(r.contains(s))
                return true;
        return false;
    }

} // namespace


// -------------------------------------------------------------------------- //
// Node
// -------------------------------------------------------------------------- //
class QnResourcePoolModel::Node {
    Q_DECLARE_TR_FUNCTIONS(Node);
public:
    enum State {
        Normal,     /**< Normal node. */
        Invalid     /**< Invalid node that should not be displayed. 
                     * Invalid nodes are parts of dangling tree branches during incremental
                     * tree construction. They do not emit model signals. */
    };

    /**
     * Constructor for toplevel nodes. 
     */
    Node(QnResourcePoolModel *model, Qn::NodeType type):
        m_model(model),
        m_type(type),
        m_state(Invalid),
        m_bastard(false),
        m_parent(NULL),
        m_status(QnResource::Online),
        m_modified(false)
    {
        assert(type == Qn::LocalNode || type == Qn::ServersNode || type == Qn::UsersNode);

        switch(type) {
        case Qn::LocalNode:
            m_name = tr("Local");
            m_icon = qnResIconCache->icon(QnResourceIconCache::Local);
            break;
        case Qn::ServersNode:
            m_name = tr("Servers");
            m_icon = qnResIconCache->icon(QnResourceIconCache::Server);
            break;
        case Qn::UsersNode:
            m_name = tr("Users");
            m_icon = qnResIconCache->icon(QnResourceIconCache::User);
            break;
        default:
            break;
        }
    }

    /**
     * Constructor for resource nodes. 
     */
    Node(QnResourcePoolModel *model, const QnResourcePtr &resource): 
        m_model(model),
        m_type(Qn::ResourceNode),
        m_state(Invalid),
        m_bastard(false),
        m_parent(NULL),
        m_status(QnResource::Offline),
        m_modified(false)
    {
        assert(model != NULL);

        setResource(resource);
    }

    /**
     * Constructor for item nodes.
     */
    Node(QnResourcePoolModel *model, const QUuid &uuid):
        m_model(model),
        m_type(Qn::ItemNode),
        m_uuid(uuid),
        m_state(Invalid),
        m_bastard(false),
        m_parent(NULL),
        m_status(QnResource::Offline),
        m_modified(false)
    {
        assert(model != NULL);
    }

    ~Node() {
        clear();

        foreach(Node *child, m_children)
            child->setParent(NULL);
    }

    void clear() {
        setParent(NULL);

        if(m_type == Qn::ItemNode || m_type == Qn::ResourceNode)
            setResource(QnResourcePtr());
    }

    void setResource(const QnResourcePtr &resource) {
        assert(m_type == Qn::ItemNode || m_type == Qn::ResourceNode);

        if(m_resource == resource)
            return;

        if(m_resource && m_type == Qn::ItemNode)
            m_model->m_itemNodesByResource[m_resource.data()].removeOne(this);

        m_resource = resource;

        if(m_resource && m_type == Qn::ItemNode)
            m_model->m_itemNodesByResource[m_resource.data()].push_back(this);
        
        update();
    }

    void update() {
        assert(m_type == Qn::ItemNode || m_type == Qn::ResourceNode);

        if(m_resource.isNull()) {
            m_name = QString();
            m_flags = 0;
            m_status = QnResource::Online;
            m_searchString = QString();
            m_icon = QIcon();
        } else {
            m_name = m_resource->getName();
            m_flags = m_resource->flags();
            m_status = m_resource->getStatus();
            m_searchString = m_resource->toSearchString();
            m_icon = qnResIconCache->icon(m_flags, m_status);
        }

        changeInternal();
    }

    Qn::NodeType type() const {
        return m_type;
    }

    const QnResourcePtr &resource() const {
        return m_resource;
    }

    QnResource::Flags resourceFlags() const {
        return m_flags;
    }

    const QUuid &uuid() const {
        return m_uuid;
    }

    State state() const {
        return m_state;
    }

    bool isValid() const {
        return m_state == Normal;
    }

    void setState(State state) {
        if(m_state == state)
            return;

        m_state = state;

        foreach(Node *node, m_children)
            node->setState(state);
    }

    bool isBastard() const {
        return m_bastard;
    }

    void setBastard(bool bastard) {
        if(m_bastard == bastard)
            return;

        m_bastard = bastard;

        if(m_parent) {
            if(m_bastard) {
                m_parent->removeChildInternal(this);
            } else {
                setState(m_parent->state());
                m_parent->addChildInternal(this);
            }
        }
    }

    const QList<Node *> &children() const {
        return m_children;
    }

    Node *child(int index) {
        return m_children[index];
    }

    Node *parent() const {
        return m_parent;
    }

    void setParent(Node *parent) {
        if(m_parent == parent)
            return;

        if(m_parent && !m_bastard)
            m_parent->removeChildInternal(this);
        
        m_parent = parent;

        if(m_parent) {
            if(!m_bastard) {
                setState(m_parent->state());
                m_parent->addChildInternal(this);
            }
        } else {
            setState(Invalid);
        }
    }

    QModelIndex index(int col) {
        assert(isValid()); /* Only valid nodes have indices. */

        if(m_parent == NULL)
            return QModelIndex(); /* That's root node. */

        return index(m_parent->m_children.indexOf(this), col);
    }

    QModelIndex index(int row, int col) {
        assert(isValid()); /* Only valid nodes have indices. */
        assert(m_parent != NULL && row == m_parent->m_children.indexOf(this));

        return m_model->createIndex(row, col, this);
    }

    Qt::ItemFlags flags() const {
        Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsSelectable;
        
        switch(m_type) {
        case Qn::ResourceNode:
        case Qn::ItemNode:
            if(m_flags & QnResource::layout)
                result |= Qt::ItemIsEditable; /* Only layouts are currently editable - user can change layout's name. */
            if(m_flags & (QnResource::media | QnResource::layout))
                result |= Qt::ItemIsDragEnabled;
            break;
        default:
            break;
        }

        return result;
    }

    QVariant data(int role, int column) const {
        switch(role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
            return m_name + (m_modified ? QLatin1String("*") : QString());
        case Qt::DecorationRole:
            if (column == 0)
                return m_icon;
            break;
        case Qt::EditRole:
            return m_name;
        case Qn::ResourceRole:
            if(m_resource)
                return QVariant::fromValue<QnResourcePtr>(m_resource);
            break;
        case Qn::ResourceFlagsRole:
            if(m_resource)
                return static_cast<int>(m_flags);
            break;
        case Qn::UuidRole:
            if(m_type == Qn::ItemNode)
                return QVariant::fromValue<QUuid>(m_uuid);
            break;
        case Qn::SearchStringRole: 
            return m_searchString;
        case Qn::StatusRole: 
            return static_cast<int>(m_status);
        case Qn::NodeTypeRole:
            return static_cast<int>(m_type);
        default:
            break;
        }

        return QVariant();        
    }

    bool setData(const QVariant &value, int role, int column) {
        Q_UNUSED(column);

        if(role != Qt::EditRole)
            return false;

        if(m_flags & QnResource::layout) {
            m_resource->setName(value.toString());
            return true;
        }

        return false;
    }

    bool isModified() const {
        return !m_modified;
    }

    void setModified(bool modified) {
        if(m_modified == modified)
            return;

        m_modified = modified; 

        changeInternal();
    }

protected:
    void removeChildInternal(Node *child) {
        assert(child->parent() == this);

        if(isValid() && !isBastard()) {
            QModelIndex index = this->index(0);
            int row = m_children.indexOf(child);

            m_model->beginRemoveRows(index, row, row);
            m_children.removeOne(child);
            m_model->endRemoveRows();
        } else {
            m_children.removeOne(child);
        }
    }

    void addChildInternal(Node *child) {
        assert(child->parent() == this);

        if(isValid() && !isBastard()) {
            QModelIndex index = this->index(0);
            int row = m_children.size();

            m_model->beginInsertRows(index, row, row);
            m_children.push_back(child);
            m_model->endInsertRows();
        } else {
            m_children.push_back(child);
        }
    }

    void changeInternal() {
        if(!isValid() || isBastard())
            return;
        
        QModelIndex index = this->index(0);
        emit m_model->dataChanged(index, index.sibling(index.row(), ColumnCount));
    }

private:
    /* Node state. */

    /** Model that this node belongs to. */
    QnResourcePoolModel *const m_model;

    /** Type of this node. */
    const Qn::NodeType m_type;

    /** Uuid that this node represents. */
    const QUuid m_uuid;

    /** Resource associated with this node. */
    QnResourcePtr m_resource;

    /** State of this node. */
    State m_state;

    /** Whether this node is a bastard node. Bastard nodes do not appear in
     * their parent's children list and do not inherit their parent's state. */
    bool m_bastard;

    /** Parent of this node. */
    Node *m_parent;

    /** Children of this node. */
    QList<Node *> m_children;


    /* Resource-related state. */

    /** Resource flags. */
    QnResource::Flags m_flags;

    /** Name of this node. */
    QString m_name;

    /** Status of this node. */
    QnResource::Status m_status;

    /** Search string of this node. */
    QString m_searchString;

    /** Icon of this node. */
    QIcon m_icon;

    /** Whether this resource is modified. */
    bool m_modified;
};


// -------------------------------------------------------------------------- //
// QnResourcePoolModel :: contructors, destructor and helpers.
// -------------------------------------------------------------------------- //
QnResourcePoolModel::QnResourcePoolModel(QObject *parent): 
    QAbstractItemModel(parent), 
    m_context(NULL),
    m_root(NULL)
{
    /* Init role names. */
    QHash<int, QByteArray> roles = roleNames();
    roles.insert(Qn::ResourceRole,      "resource");
    roles.insert(Qn::ResourceFlagsRole, "flags");
    roles.insert(Qn::UuidRole,          "uuid");
    roles.insert(Qn::SearchStringRole,  "searchString");
    roles.insert(Qn::StatusRole,        "status");
    roles.insert(Qn::NodeTypeRole,      "nodeType");
    setRoleNames(roles);

    /* Create root. */
    m_root = this->node(QnResourcePtr());
    m_root->setState(Node::Normal);

    /* Create top-level nodes. */
    m_localNode = new Node(this, Qn::LocalNode);
    m_localNode->setParent(m_root);

    m_usersNode = new Node(this, Qn::UsersNode);
    m_usersNode->setParent(m_root);

    m_serversNode = new Node(this, Qn::ServersNode);
    m_serversNode->setParent(m_root);
}

QnResourcePoolModel::~QnResourcePoolModel() {
    setContext(NULL);
    
    qDeleteAll(m_resourceNodeByResource);
    qDeleteAll(m_itemNodeByUuid);

    delete m_localNode;
    delete m_serversNode;
    delete m_usersNode;
}

void QnResourcePoolModel::setContext(QnWorkbenchContext *context) {
    if(m_context != NULL)
        stop();

    m_context = context;

    if(m_context != NULL)
        start();
}

QnWorkbenchContext *QnResourcePoolModel::context() const {
    return m_context;
}

QnResourcePool *QnResourcePoolModel::resourcePool() const {
    return m_context ? m_context->resourcePool() : NULL;
}

QnWorkbenchLayoutSnapshotManager *QnResourcePoolModel::snapshotManager() const {
    return m_context ? m_context->snapshotManager() : NULL;
}

QnResourcePtr QnResourcePoolModel::resource(const QModelIndex &index) const {
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

void QnResourcePoolModel::start() {
    assert(m_context != NULL);

    connect(m_context,          SIGNAL(aboutToBeDestroyed()),           this, SLOT(at_context_aboutToBeDestroyed()));
    connect(resourcePool(),     SIGNAL(resourceAdded(QnResourcePtr)),   this, SLOT(at_resPool_resourceAdded(QnResourcePtr)), Qt::QueuedConnection);
    connect(resourcePool(),     SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(at_resPool_resourceRemoved(QnResourcePtr)), Qt::QueuedConnection);
    connect(snapshotManager(),  SIGNAL(flagsChanged(const QnLayoutResourcePtr &)),  this, SLOT(at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &)));

    QnResourceList resources = resourcePool()->getResources(); 

    /* It is important to connect before iterating as new resources may be added to the pool asynchronously. */

    foreach(const QnResourcePtr &resource, resources)
        at_resPool_resourceAdded(resource);
}

void QnResourcePoolModel::stop() {
    assert(m_context != NULL);
    
    QnResourceList resources = resourcePool()->getResources(); 
    disconnect(m_context, NULL, this, NULL);
    disconnect(resourcePool(), NULL, this, NULL);
    disconnect(snapshotManager(), NULL, this, NULL);

    foreach(const QnResourcePtr &resource, resources)
        at_resPool_resourceRemoved(resource);
}

QnResourcePoolModel::Node *QnResourcePoolModel::node(const QnResourcePtr &resource) {
    QnResource *index = resource.data();
    QHash<QnResource *, Node *>::iterator pos = m_resourceNodeByResource.find(index);
    if(pos == m_resourceNodeByResource.end())
        pos = m_resourceNodeByResource.insert(index, new Node(this, resource));
    return *pos;
}

QnResourcePoolModel::Node *QnResourcePoolModel::node(const QUuid &uuid) {
    QHash<QUuid, Node *>::iterator pos = m_itemNodeByUuid.find(uuid);
    if(pos == m_itemNodeByUuid.end())
        pos = m_itemNodeByUuid.insert(uuid, new Node(this, uuid));
    return *pos;
}

QnResourcePoolModel::Node *QnResourcePoolModel::node(const QModelIndex &index) const {
    if(!index.isValid())
        return m_root;

    return static_cast<Node *>(index.internalPointer());
}

QnResourcePoolModel::Node *QnResourcePoolModel::expectedParent(Node *node) {
    assert(node->type() == Qn::ResourceNode);

    if(!node->resource())
        return m_root;

    if(node->resourceFlags() & QnResource::user)
        return m_usersNode;

    if(node->resourceFlags() & QnResource::server)
        return m_serversNode;

    if((node->resourceFlags() & QnResource::local_media) == QnResource::local_media)
        return m_localNode;

    return this->node(resourcePool()->getResourceById(node->resource()->getParentId()));
}

bool QnResourcePoolModel::isIgnored(const QnResourcePtr &resource) const {
    if((resource->flags() & QnResource::local_server) == QnResource::local_server)
        return true; /* Local server resource is ignored. */

    return false;
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
    return index.sibling(index.row(), 0);
}

QModelIndex QnResourcePoolModel::parent(const QModelIndex &index) const {
    if(!index.isValid())
        return QModelIndex();

    return node(index)->parent()->index(0);
}

bool QnResourcePoolModel::hasChildren(const QModelIndex &parent) const {
    return rowCount(parent) > 0;
}

int QnResourcePoolModel::rowCount(const QModelIndex &parent) const {
    if (parent.column() > 0)
        return 0;

    return node(parent)->children().size();
}

int QnResourcePoolModel::columnCount(const QModelIndex &parent) const {
    return ColumnCount;
}

Qt::ItemFlags QnResourcePoolModel::flags(const QModelIndex &index) const {
    if(!index.isValid())
        return Qt::NoItemFlags;

    return node(index)->flags();
}

QVariant QnResourcePoolModel::data(const QModelIndex &index, int role) const {
    if(!index.isValid())
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

QStringList QnResourcePoolModel::mimeTypes() const {
    QStringList result = QnWorkbenchResource::resourceMimeTypes();
    result.append(QLatin1String(pureTreeResourcesOnlyMimeType));
    return result;
}

QMimeData *QnResourcePoolModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeData = QAbstractItemModel::mimeData(indexes);
    if (mimeData) {
        const QStringList types = mimeTypes();

        bool pureTreeResourcesOnly = true;
        if (intersects(types, QnWorkbenchResource::resourceMimeTypes())) {
            QnResourceList resources;
            foreach (const QModelIndex &index, indexes) {
                Node *node = this->node(index);
                if(node && node->resource())
                    resources.append(node->resource());

                if(node && node->type() == Qn::ItemNode)
                    pureTreeResourcesOnly = false;
            }

            QnWorkbenchResource::serializeResources(resources, types, mimeData);
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
    if(!intersects(mimeData->formats(), QnWorkbenchResource::resourceMimeTypes()))
        return QAbstractItemModel::dropMimeData(mimeData, action, row, column, parent);

    /* Decode. */
    QnResourceList resources = QnWorkbenchResource::deserializeResources(mimeData);

    /* Check where we're dropping it. */
    Node *node = this->node(parent);
    
    if(node->type() == Qn::ItemNode)
        node = node->parent(); /* Dropping into an item is the same as dropping into a layout. */

    if(node->parent() && (node->parent()->resourceFlags() & QnResource::server))
        node = node->parent(); /* Dropping into a server item is the same as dropping into a server */

    if(QnLayoutResourcePtr layout = node->resource().dynamicCast<QnLayoutResource>()) {
        foreach(const QnResourcePtr &resource, resources) {
            QnMediaResourcePtr media = resource.dynamicCast<QnMediaResource>();
            if(!media)
                continue; /* Can drop only media resources on layout. */

            QnLayoutItemData item;
            item.resource.id = media->getId();
            item.uuid = QUuid::createUuid();
            item.flags = QnWorkbenchItem::PendingGeometryAdjustment;

            layout->addItem(item);
        }
    } else if(QnUserResourcePtr user = node->resource().dynamicCast<QnUserResource>()) {
        foreach(const QnResourcePtr &resource, resources) {
            if(resource->getParentId() == user->getId())
                continue; /* Dropping resource into its owner does nothing. */

            QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
            if(!layout)
                continue; /* Can drop only layout resources on user. */
            
            QVariantMap params;
            params[Qn::UserParameter] = QVariant::fromValue(user);
            params[Qn::NameParameter] = layout->getName();

            QnResourceList layouts;
            layouts.push_back(layout);

            context()->menu()->trigger(Qn::SaveLayoutAsAction, layouts, params);
        }
    } else if(QnVideoServerResourcePtr server = node->resource().dynamicCast<QnVideoServerResource>()) {
        if(mimeData->data(QLatin1String(pureTreeResourcesOnlyMimeType)) == QByteArray("1")) {
            /* Allow drop of non-layout item data, from tree only. */

            QnResourceList cameras = QnResourceCriterion::filter<QnNetworkResource, QnResourceList>(resources);
            if(!cameras.empty()) {
                QVariantMap params;
                params[Qn::ServerParameter] = QVariant::fromValue(server);

                context()->menu()->trigger(Qn::MoveCameraAction, cameras, params);
            }
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
    assert(resource && resource->getId().isValid());

    if(isIgnored(resource))
        return;

    connect(resource.data(), SIGNAL(parentIdChanged()),                                     this, SLOT(at_resource_parentIdChanged()));
    connect(resource.data(), SIGNAL(nameChanged()),                                         this, SLOT(at_resource_resourceChanged()));
    connect(resource.data(), SIGNAL(statusChanged(QnResource::Status, QnResource::Status)), this, SLOT(at_resource_resourceChanged()));
    connect(resource.data(), SIGNAL(resourceChanged()),                                     this, SLOT(at_resource_resourceChanged()));

    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    if(layout) {
        connect(layout.data(), SIGNAL(itemAdded(const QnLayoutItemData &)),                 this, SLOT(at_resource_itemAdded(const QnLayoutItemData &)));
        connect(layout.data(), SIGNAL(itemRemoved(const QnLayoutItemData &)),               this, SLOT(at_resource_itemRemoved(const QnLayoutItemData &)));
    }

    Node *node = this->node(resource);
    node->setResource(resource);

    at_resource_parentIdChanged(resource);
    at_resource_resourceChanged(resource);

    if(layout)
        foreach(const QnLayoutItemData &item, layout->getItems())
            at_resource_itemAdded(layout, item);
}

void QnResourcePoolModel::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    if(isIgnored(resource))
        return;

    disconnect(resource.data(), NULL, this, NULL);

    Node *node = this->node(resource);
    node->clear();

    // TODO: delete node here?
}

void QnResourcePoolModel::at_context_aboutToBeDestroyed() {
    setContext(NULL);
}

void QnResourcePoolModel::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource) {
    Node *node = this->node(resource);

    node->setModified(snapshotManager()->isModified(resource));
}

void QnResourcePoolModel::at_resource_parentIdChanged(const QnResourcePtr &resource) {
    Node *node = this->node(resource);

    node->setParent(expectedParent(node));
}

void QnResourcePoolModel::at_resource_parentIdChanged() {
    QObject *sender = this->sender();
    if(!sender)
        return; /* Already disconnected from this sender. */

    at_resource_parentIdChanged(toSharedPointer(checked_cast<QnResource *>(sender)));
}

void QnResourcePoolModel::at_resource_resourceChanged(const QnResourcePtr &resource) {
    QnResource::Status status = resource->getStatus();

    node(resource)->update();
    node(resource)->setBastard(status == QnResource::Disabled);
    foreach(Node *node, m_itemNodesByResource[resource.data()])
        node->update();
}

void QnResourcePoolModel::at_resource_resourceChanged() {
    QObject *sender = this->sender();
    if(!sender)
        return; /* Already disconnected from this sender. */

    at_resource_resourceChanged(toSharedPointer(checked_cast<QnResource *>(sender)));
}

void QnResourcePoolModel::at_resource_itemAdded(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item) {
    Node *parentNode = this->node(layout);
    Node *node = this->node(item.uuid);

    QnResourcePtr resource;
    if(item.resource.id.isValid()) {
        resource = resourcePool()->getResourceById(item.resource.id);
    } else {
        resource = resourcePool()->getResourceByUniqId(item.resource.path);
    }

    node->setResource(resource);
    node->setParent(parentNode);
}

void QnResourcePoolModel::at_resource_itemAdded(const QnLayoutItemData &item) {
    QObject *sender = this->sender();
    if(!sender)
        return; /* Already disconnected from this sender. */

    at_resource_itemAdded(toSharedPointer(checked_cast<QnLayoutResource *>(sender)), item);
}

void QnResourcePoolModel::at_resource_itemRemoved(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item) {
    Node *node = this->node(item.uuid);
    node->clear();
}

void QnResourcePoolModel::at_resource_itemRemoved(const QnLayoutItemData &item) {
    QObject *sender = this->sender();
    if(!sender)
        return; /* Already disconnected from this sender. */

    at_resource_itemRemoved(toSharedPointer(checked_cast<QnLayoutResource *>(sender)), item);
}

















