#include "resource_model.h"
#include <cassert>
#include <QMimeData>
#include <QUrl>
#include <utils/common/checked_cast.h>
#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <ui/view_drag_and_drop.h>
#include <ui/style/resource_icon_cache.h>
#include "file_processor.h"

namespace {
    enum Columns {
        NameColumn,
        ColumnCount
    };

} // namespace


// -------------------------------------------------------------------------- //
// Node
// -------------------------------------------------------------------------- //
class QnResourceModel::Node {
public:
    enum Type {
        Resource,   /**< Node that represents a resource. */
        Item,       /**< Node that represents a layout item. */
    };

    enum State {
        Normal,     /**< Normal node. */
        Invalid     /**< Invalid node that should not be displayed. 
                     * Invalid nodes may be parts of dangling tree branches during incremental
                     * tree construction. They do not emit model signals. */
    };

    /**
     * Constructor for resource nodes. 
     */
    Node(QnResourceModel *model, const QnId &id): 
        m_model(model),
        m_type(Resource),
        m_id(id), 
        m_state(Invalid),
        m_parent(NULL)
    {
        assert(model != NULL);
    }

    /**
     * Constructor for item nodes.
     */
    Node(QnResourceModel *model, const QUuid &uuid):
        m_model(model),
        m_type(Item),
        m_id(0),
        m_uuid(uuid),
        m_state(Invalid),
        m_parent(NULL)
    {
        assert(model != NULL);
    }

    ~Node() {
        clear();
    }

    void clear() {
        setParent(NULL);
        setResource(QnResourcePtr());
    }

    void setResource(const QnResourcePtr &resource) {
        if(m_resource == resource)
            return;

        m_resource = resource;
        update();
    }

    void update() {
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

            if(m_type == Item) {
                setId(m_resource->getId());
            } else {
                assert(m_id == m_resource->getId());
            }
        }

        changeInternal();
    }

    const QnId &id() const {
        return m_id;
    }

    void setId(const QnId &id) {
        assert(m_type == Item);

        if(m_id == id)
            return;

        if(m_id.isValid())
            m_model->m_itemNodesById[m_id].removeOne(this);

        m_id = id;

        if(m_id.isValid())
            m_model->m_itemNodesById[m_id].push_back(this);
    }

    const QUuid &uuid() const {
        return m_uuid;
    }

    State state() const {
        return m_state;
    }

    bool isValid() const {
        return m_state != Invalid;
    }

    void setState(State state) {
        if(m_state == state)
            return;

        m_state = state;

        foreach(Node *node, m_children)
            node->setState(state);
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

        if(m_parent)
            m_parent->removeChildInternal(this);
        
        m_parent = parent;

        if(m_parent) {
            setState(m_parent->state());
            m_parent->addChildInternal(this);
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
        case Resource:
        case Item:
            if(m_flags & QnResource::media)
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
            return m_name;
        case Qt::DecorationRole:
            if (column == 0)
                return m_icon;
            break;
        case Qt::EditRole:
            break;
        case Qn::ResourceRole:
            if(!m_resource.isNull())
                return QVariant::fromValue<QnResourcePtr>(m_resource);
            break;
        case Qn::ResourceFlagsRole:
            if(!m_resource.isNull())
                return static_cast<int>(m_flags);
            break;
        case Qn::IdRole: 
            if(m_id.isValid())
                return QVariant::fromValue<QnId>(m_id);
            break;
        case Qn::UuidRole:
            if(m_type == Item)
                return QVariant::fromValue<QUuid>(m_uuid);
            break;
        case Qn::SearchStringRole: 
            return m_searchString;
        case Qn::StatusRole: 
            return static_cast<int>(m_status);
        default:
            break;
        }

        return QVariant();        
    }

protected:
    void removeChildInternal(Node *child) {
        assert(child->parent() == this);

        if(isValid()) {
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

        if(isValid()) {
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
        if(!isValid())
            return;
        
        QModelIndex index = this->index(0);
        emit m_model->dataChanged(index, index.sibling(index.row(), ColumnCount));
    }

private:
    /* Node state. */

    /** Model that this node belongs to. */
    QnResourceModel *const m_model;

    /** Type of this node. */
    const Type m_type;

    /** Uuid that this node represents. */
    const QUuid m_uuid;

    /** Id that this node represents. */
    QnId m_id;

    /** Whether this node is valid, i.e. represents an entity that should be
     * visible in the view.
     * 
     *  */
    State m_state;

    /** Parent of this node. */
    Node *m_parent;

    /** Children of this node. */
    QList<Node *> m_children;


    /* Resource-related state. */

    /** Resource associated with this node. */
    QnResourcePtr m_resource;

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
};


// -------------------------------------------------------------------------- //
// QnResourceModel :: contructors, destructor and helpers.
// -------------------------------------------------------------------------- //
QnResourceModel::QnResourceModel(QObject *parent): 
    QAbstractItemModel(parent), 
    m_resourcePool(NULL),
    m_root(NULL)
{
    /* Init role names. */
    QHash<int, QByteArray> roles = roleNames();
    roles.insert(Qn::ResourceRole,      "resource");
    roles.insert(Qn::ResourceFlagsRole, "flags");
    roles.insert(Qn::IdRole,            "id");
    roles.insert(Qn::UuidRole,          "uuid");
    roles.insert(Qn::SearchStringRole,  "searchString");
    roles.insert(Qn::StatusRole,        "status");
    setRoleNames(roles);

    /* Create root. */
    m_root = this->node(QnId());;
    m_root->setState(Node::Normal);
}

QnResourceModel::~QnResourceModel() {
    setResourcePool(NULL);
    delete m_root;
}

void QnResourceModel::setResourcePool(QnResourcePool *resourcePool) {
    if(m_resourcePool != NULL)
        stop();

    m_resourcePool = resourcePool;

    if(m_resourcePool != NULL)
        start();
}

QnResourcePool *QnResourceModel::resourcePool() const {
    return m_resourcePool;
}

QnResourcePtr QnResourceModel::resource(const QModelIndex &index) const {
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

void QnResourceModel::start() {
    assert(m_resourcePool != NULL);

    connect(m_resourcePool, SIGNAL(resourceAdded(QnResourcePtr)),   this, SLOT(at_resPool_resourceAdded(QnResourcePtr)));
    connect(m_resourcePool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(at_resPool_resourceRemoved(QnResourcePtr)));
    connect(m_resourcePool, SIGNAL(aboutToBeDestroyed()),           this, SLOT(at_resPool_aboutToBeDestroyed()));
    QnResourceList resources = m_resourcePool->getResources(); 

    /* It is important to connect before iterating as new resources may be added to the pool asynchronously. */

    foreach(const QnResourcePtr &resource, resources)
        at_resPool_resourceAdded(resource);
}

void QnResourceModel::stop() {
    assert(m_resourcePool != NULL);
    
    QnResourceList resources = m_resourcePool->getResources(); 
    disconnect(m_resourcePool, NULL, this, NULL);

    foreach(const QnResourcePtr &resource, resources)
        at_resPool_resourceRemoved(resource);
}

QnResourceModel::Node *QnResourceModel::node(const QnId &id) {
    QHash<QnId, Node *>::iterator pos = m_resourceNodeById.find(id);
    if(pos == m_resourceNodeById.end())
        pos = m_resourceNodeById.insert(id, new Node(this, id));
    return *pos;
}

QnResourceModel::Node *QnResourceModel::node(const QUuid &uuid) {
    QHash<QUuid, Node *>::iterator pos = m_itemNodeByUuid.find(uuid);
    if(pos == m_itemNodeByUuid.end())
        pos = m_itemNodeByUuid.insert(uuid, new Node(this, uuid));
    return *pos;
}

QnResourceModel::Node *QnResourceModel::node(const QModelIndex &index) const {
    if(!index.isValid())
        return m_root;

    return static_cast<Node *>(index.internalPointer());
}


// -------------------------------------------------------------------------- //
// QnResourceModel :: QAbstractItemModel implementation
// -------------------------------------------------------------------------- //
QModelIndex QnResourceModel::index(int row, int column, const QModelIndex &parent) const {
    if(!hasIndex(row, column, parent)) /* hasIndex calls rowCount and columnCount. */
        return QModelIndex();

    return node(parent)->child(row)->index(row, column);
}

QModelIndex QnResourceModel::buddy(const QModelIndex &index) const {
    return index.sibling(index.row(), 0);
}

QModelIndex QnResourceModel::parent(const QModelIndex &index) const {
    if(!index.isValid())
        return QModelIndex();

    return node(index)->parent()->index(0);
}

bool QnResourceModel::hasChildren(const QModelIndex &parent) const {
    return rowCount(parent) > 0;
}

int QnResourceModel::rowCount(const QModelIndex &parent) const {
    if (parent.column() > 0)
        return 0;

    return node(parent)->children().size();
}

int QnResourceModel::columnCount(const QModelIndex &parent) const {
    return ColumnCount;
}

Qt::ItemFlags QnResourceModel::flags(const QModelIndex &index) const {
    if(!index.isValid())
        return Qt::NoItemFlags;

    return node(index)->flags();
}

QVariant QnResourceModel::data(const QModelIndex &index, int role) const {
    if(!index.isValid())
        return QVariant();

    return node(index)->data(role, index.column());
}

bool QnResourceModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    return false; /* Resource model is not editable. */
}

QVariant QnResourceModel::headerData(int section, Qt::Orientation orientation, int role) const {
    return QVariant(); /* No headers needed. */
}

QStringList QnResourceModel::mimeTypes() const {
    QStringList mimeTypes = QAbstractItemModel::mimeTypes();
    mimeTypes.append(QLatin1String("text/uri-list"));
    mimeTypes.append(resourcesMime());

    return mimeTypes;
}

QMimeData *QnResourceModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeData = QAbstractItemModel::mimeData(indexes);
    if (mimeData) {
        const QStringList types = mimeTypes();
        const QString resourceFormat = resourcesMime();
        const QString urlFormat = QLatin1String("text/uri-list");
        if (types.contains(resourceFormat) || types.contains(urlFormat)) {
            QnResourceList resources;
            foreach (const QModelIndex &index, indexes) {
                if (QnResourcePtr resource = this->resource(index))
                    resources.append(resource);
            }
            if (types.contains(resourceFormat))
                mimeData->setData(resourceFormat, serializeResources(resources));
            if (types.contains(urlFormat)) {
                QList<QUrl> urls;
                foreach (const QnResourcePtr &resource, resources) {
                    if (resource->checkFlags(QnResource::url))
                        urls.append(QUrl::fromLocalFile(resource->getUrl()));
                }
                mimeData->setUrls(urls);
            }
        }
    }

    return mimeData;
}

bool QnResourceModel::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
    if (!mimeData)
        return false;

    /* Check if the action is supported. */
    if(!(action & supportedDropActions()))
        return false;

    /* Check if the format is supported. */
    const QString format = resourcesMime();
    if (!mimeData->hasFormat(format) && !mimeData->hasUrls())
        return QAbstractItemModel::dropMimeData(mimeData, action, row, column, parent);

    /* Decode. */
    QnResourceList resources;
    if (!mimeData->hasFormat(format)) {
        resources += deserializeResources(mimeData->data(format));
    } else if (mimeData->hasUrls()) {
        resources += QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(mimeData->urls()));
    }

    /* Insert. Resources will be inserted into this model in callbacks. */
    qnResPool->addResources(resources);
    
    return true;
}

Qt::DropActions QnResourceModel::supportedDropActions() const {
    return Qt::CopyAction | Qt::MoveAction;
}


// -------------------------------------------------------------------------- //
// QnResourceModel :: handlers
// -------------------------------------------------------------------------- //
void QnResourceModel::at_resPool_resourceAdded(const QnResourcePtr &resource) {
    assert(resource && resource->getId().isValid());

    connect(resource.data(), SIGNAL(parentIdChanged()),                                     this, SLOT(at_resource_parentIdChanged()));
    connect(resource.data(), SIGNAL(nameChanged()),                                         this, SLOT(at_resource_resourceChanged()));
    connect(resource.data(), SIGNAL(statusChanged(QnResource::Status, QnResource::Status)), this, SLOT(at_resource_resourceChanged()));
    connect(resource.data(), SIGNAL(resourceChanged()),                                     this, SLOT(at_resource_resourceChanged()));

    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    if(layout) {
        connect(layout.data(), SIGNAL(itemAdded(const QnLayoutItemData &)),                 this, SLOT(at_resource_itemAdded(const QnLayoutItemData &)));
        connect(layout.data(), SIGNAL(itemRemoved(const QnLayoutItemData &)),               this, SLOT(at_resource_itemRemoved(const QnLayoutItemData &)));
    }

    Node *node = this->node(resource->getId());
    node->setResource(resource);

    at_resource_parentIdChanged(resource);

    if(layout)
        foreach(const QnLayoutItemData &item, layout->getItems())
            at_resource_itemAdded(layout, item);
}

void QnResourceModel::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    Node *node = this->node(resource->getId());
    node->clear();
}

void QnResourceModel::at_resPool_aboutToBeDestroyed() {
    setResourcePool(NULL);
}

void QnResourceModel::at_resource_parentIdChanged(const QnResourcePtr &resource) {
    Node *node = this->node(resource->getId());
    Node *parentNode = this->node(resource->getParentId());

    node->setParent(parentNode);
}

void QnResourceModel::at_resource_parentIdChanged() {
    at_resource_parentIdChanged(toSharedPointer(checked_cast<QnResource *>(sender())));
}

void QnResourceModel::at_resource_resourceChanged() {
    QnResourcePtr resource = toSharedPointer(checked_cast<QnResource *>(sender()));

    node(resource->getId())->update();
    foreach(Node *node, m_itemNodesById[resource->getId()])
        node->update();
}

void QnResourceModel::at_resource_itemAdded(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item) {
    Node *parentNode = this->node(layout->getId());
    Node *node = this->node(item.uuid);

    QnResourcePtr resource;
    if(item.resource.id.isValid()) {
        resource = m_resourcePool->getResourceById(item.resource.id);
    } else {
        resource = m_resourcePool->getResourceByUniqId(item.resource.path);
    }

    node->setResource(resource);
    node->setParent(parentNode);
}

void QnResourceModel::at_resource_itemAdded(const QnLayoutItemData &item) {
    at_resource_itemAdded(toSharedPointer(checked_cast<QnLayoutResource *>(sender())), item);
}

void QnResourceModel::at_resource_itemRemoved(const QnLayoutResourcePtr &layout, const QnLayoutItemData &item) {
    Node *node = this->node(item.uuid);
    node->clear();
}

void QnResourceModel::at_resource_itemRemoved(const QnLayoutItemData &item) {
    at_resource_itemRemoved(toSharedPointer(checked_cast<QnLayoutResource *>(sender())), item);
}

















