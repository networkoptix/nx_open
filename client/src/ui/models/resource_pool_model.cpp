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
#include <core/resource_managment/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_resource.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_access_controller.h>

#include "file_processor.h"

namespace {
    enum Columns {
        NameColumn,
        CheckColumn,
        ColumnCount
    };

    const char *pureTreeResourcesOnlyMimeType = "application/x-noptix-pure-tree-resources-only";

    bool intersects(const QStringList &l, const QStringList &r) {
        foreach(const QString &s, l)
            if(r.contains(s))
                return true;
        return false;
    }

    QString extractHost(const QString &url) {
        /* Try it as a host address first. */
        QHostAddress hostAddress(url);
        if(!hostAddress.isNull())
            return hostAddress.toString();

        /* Then go default QUrl route. */
        return QUrl(url).host();
    }

} // namespace


// -------------------------------------------------------------------------- //
// Node
// -------------------------------------------------------------------------- //
class QnResourcePoolModel::Node {
    Q_DECLARE_TR_FUNCTIONS(Node)
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
        m_state(Normal),
        m_bastard(false),
        m_parent(NULL),
        m_status(QnResource::Online),
        m_modified(false),
        m_checked(Qt::Unchecked)
    {
        assert(type == Qn::LocalNode || type == Qn::ServersNode || type == Qn::UsersNode || type == Qn::RootNode || type == Qn::BastardNode);

        switch(type) {
        case Qn::RootNode:
            m_displayName = m_name = tr("Root");
            break;
        case Qn::LocalNode:
            m_displayName = m_name = tr("Local");
            m_icon = qnResIconCache->icon(QnResourceIconCache::Local);
            break;
        case Qn::ServersNode:
            m_displayName = m_name = tr("Servers");
            m_icon = qnResIconCache->icon(QnResourceIconCache::Servers);
            break;
        case Qn::UsersNode:
            m_displayName = m_name = tr("Users");
            m_icon = qnResIconCache->icon(QnResourceIconCache::Users);
            break;
        case Qn::BastardNode:
            m_displayName = m_name = QLatin1String("_HIDDEN_"); // this node is always hidden
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
        m_modified(false),
        m_checked(Qt::Unchecked)
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
        m_modified(false),
        m_checked(Qt::Unchecked)
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
        /* Update stored fields. */
        if(m_type == Qn::ResourceNode || m_type == Qn::ItemNode) {
            if(m_resource.isNull()) {
                m_displayName = m_name = QString();
                m_flags = 0;
                m_status = QnResource::Online;
                m_searchString = QString();
                m_icon = QIcon();
            } else {
                m_displayName = m_name = m_resource->getName();
                m_flags = m_resource->flags();
                m_status = m_resource->getStatus();
                m_searchString = m_resource->toSearchString();
                m_icon = qnResIconCache->icon(m_flags, m_status);

                if(m_model->m_urlsShown) {
                    if((m_flags & QnResource::network) || (m_flags & QnResource::server && m_flags & QnResource::remote)) {
                        QString host = extractHost(m_resource->getUrl());
                        if(!host.isEmpty())
                            m_displayName = tr("%1 (%2)").arg(m_name).arg(host);
                    }
                }
            }
        }

        /* Update bastard state. */
        bool bastard = false;
        switch(m_type) {
        case Qn::ItemNode:
            bastard = m_resource.isNull();
            break;
        case Qn::ResourceNode: 
            bastard = !(m_model->accessController()->permissions(m_resource) & Qn::ReadPermission); /* Hide non-readable resources. */
            if(!bastard)
                if(QnLayoutResourcePtr layout = m_resource.dynamicCast<QnLayoutResource>()) /* Hide local layouts that are not file-based. */
                    bastard = m_model->snapshotManager()->isLocal(layout) && !m_model->snapshotManager()->isFile(layout); 
            if(!bastard)
                bastard = (m_flags & QnResource::local_server) == QnResource::local_server; /* Hide local server resource. */
            if(!bastard)
                bastard = (m_flags & QnResource::local_media) == QnResource::local_media && m_resource->getUrl().startsWith(QLatin1String("layout://")); // TODO: hack hack hack
            break;
        case Qn::UsersNode:
            bastard = !m_model->accessController()->hasGlobalPermissions(Qn::GlobalEditUsersPermission);
            break;
        case Qn::ServersNode:
            bastard = !m_model->accessController()->hasGlobalPermissions(Qn::GlobalEditServersPermissions);
            break;
        case Qn::BastardNode:
            bastard = true;
            break;
        default:
            break;
        }
        setBastard(bastard);

        /* Notify views. */
        changeInternal();
    }

    void updateRecursive() {
        update();

        foreach(Node *child, m_children)
            child->updateRecursive();
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

    QnResource::Status resourceStatus() const {
        return m_status;
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

    Qt::ItemFlags flags(int column) const {
        if (column== CheckColumn)
            return Qt::ItemIsEnabled
                    | Qt::ItemIsSelectable
                    | Qt::ItemIsUserCheckable
                    | Qt::ItemIsEditable
                    | Qt::ItemIsTristate;

        Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsSelectable;
        
        switch(m_type) {
        case Qn::ResourceNode:
            if(m_model->context()->menu()->canTrigger(Qn::RenameAction, QnActionParameters(m_resource)))
                result |= Qt::ItemIsEditable;
            /* Fall through. */
        case Qn::ItemNode:
            if(m_flags & (QnResource::media | QnResource::layout | QnResource::server))
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
            if (column == NameColumn)
                return m_displayName + (m_modified ? QLatin1String("*") : QString());
            break;
        case Qt::DecorationRole:
            if (column == NameColumn)
                return m_icon;
            break;
        case Qt::EditRole:
            if (column == NameColumn)
                return m_name;
            break;
        case Qt::CheckStateRole:
            if (column == CheckColumn)
                return m_checked;
            break;
        case Qn::ResourceRole:
            if(m_resource)
                return QVariant::fromValue<QnResourcePtr>(m_resource);
            break;
        case Qn::ResourceFlagsRole:
            if(m_resource)
                return static_cast<int>(m_flags);
            break;
        case Qn::ItemUuidRole:
            if(m_type == Qn::ItemNode)
                return QVariant::fromValue<QUuid>(m_uuid);
            break;
        case Qn::ResourceSearchStringRole: 
            return m_searchString;
        case Qn::ResourceStatusRole: 
            return static_cast<int>(m_status);
        case Qn::NodeTypeRole:
            return static_cast<int>(m_type);
        case Qn::HelpTopicIdRole:
            if(m_flags & QnResource::layout) {
                return Qn::MainWindow_Tree_Layouts_Help;
            } else if(m_flags & QnResource::user) {
                return Qn::MainWindow_Tree_Users_Help;
            } else if(m_flags & QnResource::local) {
                return Qn::MainWindow_Tree_Local_Help;
            } else if(m_flags & QnResource::server) {
                return Qn::MainWindow_Tree_Servers_Help;
            } else {
                return -1;
            }
        default:
            break;
        }

        return QVariant();        
    }

    bool setData(const QVariant &value, int role, int column) {
        if (column == CheckColumn && role == Qt::CheckStateRole){
            Qt::CheckState checkState = (Qt::CheckState)value.toInt();
            setCheckStateRecursive(checkState);
            if (m_parent)
                m_parent->updateParentCheckStateRecursive(checkState);
            return true;
        }

        if(role != Qt::EditRole)
            return false;

        m_model->context()->menu()->trigger(Qn::RenameAction, QnActionParameters(m_resource).withArgument(Qn::NameParameter, value.toString()));
        return true;
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
            QModelIndex index = this->index(NameColumn);
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
            QModelIndex index = this->index(NameColumn);
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
        
        QModelIndex index = this->index(NameColumn);
        emit m_model->dataChanged(index, index.sibling(index.row(), ColumnCount - 1));
    }

    void setCheckStateRecursive(Qt::CheckState state){
        m_checked = state;
      /*  foreach(Node* child, m_children)
            child->setCheckStateRecursive(state);*/
        changeInternal();
    }

    void updateParentCheckStateRecursive(Qt::CheckState state){
      /*  foreach(Node* child, m_children)
            if (child->m_checked != state) {
                m_checked = Qt::Unchecked;
                if (m_parent)
                    m_parent->updateParentCheckStateRecursive(Qt::Unchecked);
                return;
            }
        m_checked = state;
        if (m_parent)
            m_parent->updateParentCheckStateRecursive(state);
        changeInternal();*/
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

    /** Display name of this node */
    QString m_displayName;

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

    /** Whether this resource is checked. */
    Qt::CheckState m_checked;
};


// -------------------------------------------------------------------------- //
// QnResourcePoolModel :: contructors, destructor and helpers.
// -------------------------------------------------------------------------- //
QnResourcePoolModel::QnResourcePoolModel(QObject *parent, Qn::NodeType rootNodeType):
    QAbstractItemModel(parent), 
    QnWorkbenchContextAware(parent),
    m_urlsShown(true),
    m_rootNodeType(rootNodeType)
{
    /* Init role names. */
    QHash<int, QByteArray> roles = roleNames();
    roles.insert(Qn::ResourceRole,              "resource");
    roles.insert(Qn::ResourceFlagsRole,         "flags");
    roles.insert(Qn::ItemUuidRole,              "uuid");
    roles.insert(Qn::ResourceSearchStringRole,  "searchString");
    roles.insert(Qn::ResourceStatusRole,        "status");
    roles.insert(Qn::NodeTypeRole,              "nodeType");
    setRoleNames(roles);

    // TODO: #gdm looks like we need an array indexed by Qn::NodeType here,
    // it will make the code shorter.

    /* Create top-level nodes. */
    m_localNode = new Node(this, Qn::LocalNode);
    m_usersNode = new Node(this, Qn::UsersNode);
    m_serversNode = new Node(this, Qn::ServersNode);
    m_bastardNode = new Node(this, Qn::BastardNode);

    /* Create root. */
    switch (rootNodeType) {
    case Qn::LocalNode:
        m_rootNode = m_localNode;
        break;
    case Qn::UsersNode:
        m_rootNode = m_usersNode;
        break;
    case Qn::ServersNode:
        m_rootNode = m_serversNode;
        break;
    default:
        Q_ASSERT(rootNodeType == Qn::RootNode);
        m_rootNode = new Node(this, Qn::RootNode);
        break;
    }

    if (rootNodeType != Qn::LocalNode)
        m_localNode->setParent(rootNodeType == Qn::RootNode ? m_rootNode : m_bastardNode);
    if (rootNodeType != Qn::UsersNode)
        m_usersNode->setParent(rootNodeType == Qn::RootNode ? m_rootNode : m_bastardNode);
    if (rootNodeType != Qn::ServersNode)
        m_serversNode->setParent(rootNodeType == Qn::RootNode ? m_rootNode : m_bastardNode);


    /* Connect to context. */
    connect(resourcePool(),     SIGNAL(resourceAdded(QnResourcePtr)),   this, SLOT(at_resPool_resourceAdded(QnResourcePtr)), Qt::QueuedConnection);
    connect(resourcePool(),     SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(at_resPool_resourceRemoved(QnResourcePtr)), Qt::QueuedConnection);
    connect(snapshotManager(),  SIGNAL(flagsChanged(const QnLayoutResourcePtr &)),  this, SLOT(at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &)));
    connect(accessController(), SIGNAL(permissionsChanged(const QnResourcePtr &)),  this, SLOT(at_accessController_permissionsChanged(const QnResourcePtr &)));
    connect(context(),          SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(at_context_userChanged()), Qt::QueuedConnection);

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

    if (m_rootNodeType == Qn::RootNode)
        delete m_rootNode;
    delete m_localNode;
    delete m_serversNode;
    delete m_usersNode;
    delete m_bastardNode;
}

QnResourcePtr QnResourcePoolModel::resource(const QModelIndex &index) const {
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

QnResourcePoolModel::Node *QnResourcePoolModel::node(const QnResourcePtr &resource) {
    QnResource *index = resource.data();
    if(!index)
        return m_rootNode;

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
        return m_rootNode;

    return static_cast<Node *>(index.internalPointer());
}

QnResourcePoolModel::Node *QnResourcePoolModel::expectedParent(Node *node) {
    assert(node->type() == Qn::ResourceNode);

    if(!node->resource())
        return m_rootNode;

    if(node->resourceFlags() & QnResource::user) {
        if(!accessController()->hasGlobalPermissions(Qn::GlobalEditUsersPermission)) {
            return m_rootNode;
        } else {
            return m_usersNode;
        }
    }

    if(node->resourceFlags() & QnResource::server)
        return m_serversNode;

    QnResourcePtr parentResource = resourcePool()->getResourceById(node->resource()->getParentId());
    if(!parentResource || (parentResource->flags() & QnResource::local_server) == QnResource::local_server) {
        if(node->resourceFlags() & QnResource::local) {
            return m_localNode;
        } else {
            return NULL;
        }
    } else {
        return this->node(parentResource);
    }
}

bool QnResourcePoolModel::isUrlsShown() {
    return m_urlsShown;
}

void QnResourcePoolModel::setUrlsShown(bool urlsShown) {
    if(urlsShown == m_urlsShown)
        return;

    m_urlsShown = urlsShown;

    m_rootNode->updateRecursive();
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
    if(!index.isValid())
        return QModelIndex();

    return node(index)->parent()->index(NameColumn);
}

bool QnResourcePoolModel::hasChildren(const QModelIndex &parent) const {
    return rowCount(parent) > 0;
}

int QnResourcePoolModel::rowCount(const QModelIndex &parent) const {
    // TODO: #gdm 
    // Only children of the first column are considered when TreeView is
    // building a tree.
    // You should always return zero for other columns,
    // so the condition should be (parent.column() >= 0).
    if (parent.column() >= ColumnCount)
        return 0;

    return node(parent)->children().size();
}

int QnResourcePoolModel::columnCount(const QModelIndex &/*parent*/) const {
    return ColumnCount;
}

Qt::ItemFlags QnResourcePoolModel::flags(const QModelIndex &index) const {
    if(!index.isValid())
        return Qt::NoItemFlags;
    return node(index)->flags(index.column());
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
        QnMediaResourceList medias = resources.filtered<QnMediaResource>();

        menu()->trigger(Qn::OpenInLayoutAction, QnActionParameters(medias).withArgument(Qn::LayoutParameter, layout));
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
                    withArgument(Qn::UserParameter, user).
                    withArgument(Qn::NameParameter, layout->getName())
            );
        }
    } else if(QnMediaServerResourcePtr server = node->resource().dynamicCast<QnMediaServerResource>()) {
        if(mimeData->data(QLatin1String(pureTreeResourcesOnlyMimeType)) == QByteArray("1")) {
            /* Allow drop of non-layout item data, from tree only. */

            QnNetworkResourceList cameras = resources.filtered<QnNetworkResource>();
            if(!cameras.empty())
                menu()->trigger(Qn::MoveCameraAction, QnActionParameters(cameras).withArgument(Qn::ServerParameter, server));
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

    connect(resource.data(), SIGNAL(parentIdChanged()),                                     this, SLOT(at_resource_parentIdChanged()));
    connect(resource.data(), SIGNAL(nameChanged()),                                         this, SLOT(at_resource_resourceChanged()));
    connect(resource.data(), SIGNAL(statusChanged(QnResource::Status, QnResource::Status)), this, SLOT(at_resource_resourceChanged()));
    connect(resource.data(), SIGNAL(disabledChanged(bool, bool)),                           this, SLOT(at_resource_resourceChanged()));
    connect(resource.data(), SIGNAL(urlChanged()),                                          this, SLOT(at_resource_resourceChanged()));
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
    disconnect(resource.data(), NULL, this, NULL);

    Node *node = this->node(resource);
    node->clear();

    // TODO: delete node here?
}

void QnResourcePoolModel::at_context_userChanged() {
    m_localNode->update();
    m_serversNode->update();
    m_usersNode->update();

    foreach(Node *node, m_resourceNodeByResource)
        node->setParent(expectedParent(node));
}

void QnResourcePoolModel::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource) {
    Node *node = this->node(resource);

    node->setModified(snapshotManager()->isModified(resource));
    node->update();
}

void QnResourcePoolModel::at_accessController_permissionsChanged(const QnResourcePtr &resource) {
    node(resource)->update();
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
    node(resource)->update();

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

void QnResourcePoolModel::at_resource_itemRemoved(const QnLayoutResourcePtr &, const QnLayoutItemData &item) {
    Node *node = this->node(item.uuid);
    node->clear();
}

void QnResourcePoolModel::at_resource_itemRemoved(const QnLayoutItemData &item) {
    QObject *sender = this->sender();
    if(!sender)
        return; /* Already disconnected from this sender. */

    at_resource_itemRemoved(toSharedPointer(checked_cast<QnLayoutResource *>(sender)), item);
}

















