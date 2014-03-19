#include "resource_pool_model.h"
#include <cassert>

#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>

#include <utils/common/checked_cast.h>
#include <common/common_meta_types.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/file_processor.h>
#include <core/resource_management/resource_pool.h>

#include "plugins/storage/file_storage/layout_storage_resource.h"

#include <ui/actions/action_manager.h>
#include <ui/common/ui_resource_name.h>
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
// Node
// -------------------------------------------------------------------------- //
class QnResourcePoolModel::Node {
    Q_DECLARE_TR_FUNCTIONS(QnResourcePoolModel::Node)
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
    Node(QnResourcePoolModel *model, Qn::NodeType type, const QString &name = QString()):
        m_model(model),
        m_type(type),
        m_state(Normal),
        m_bastard(false),
        m_parent(NULL),
        m_status(QnResource::Online),
        m_modified(false),
        m_checked(Qt::Unchecked)
    {
        assert(type == Qn::LocalNode ||
               type == Qn::ServersNode ||
               type == Qn::UsersNode ||
               type == Qn::RootNode ||
               type == Qn::BastardNode ||
               type == Qn::RecorderNode);

        switch(type) {
        case Qn::RootNode:
            m_displayName = m_name = tr("Root");
            break;
        case Qn::LocalNode:
            m_displayName = m_name = tr("Local");
            m_icon = qnResIconCache->icon(QnResourceIconCache::Local);
            break;
        case Qn::ServersNode:
            m_displayName = m_name = tr("System");
            m_icon = qnResIconCache->icon(QnResourceIconCache::Servers);
            break;
        case Qn::UsersNode:
            m_displayName = m_name = tr("Users");
            m_icon = qnResIconCache->icon(QnResourceIconCache::Users);
            break;
        case Qn::BastardNode:
            m_displayName = m_name = QLatin1String("_HIDDEN_"); /* This node is always hidden. */
            m_bastard = true;
            break;
        case Qn::RecorderNode:
            m_displayName = m_name = name;
            m_icon = qnResIconCache->icon(QnResourceIconCache::Recorder);
            m_bastard = true; /* Invisible by default until has children. */
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
                m_name = m_resource->getName();
                m_flags = m_resource->flags();
                m_status = m_resource->getStatus();
                m_searchString = m_resource->toSearchString();
                m_icon = qnResIconCache->icon(m_flags, m_status);
                m_displayName = getResourceName(m_resource);
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
                bastard = (m_flags & QnResource::local_media) == QnResource::local_media &&
                        m_resource->getUrl().startsWith(QnLayoutFileStorageResource::layoutPrefix()); //TODO: #Elric hack hack hack
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
        case Qn::RecorderNode:
            bastard = m_children.size() == 0;
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
        if (column == Qn::CheckColumn)
            return Qt::ItemIsEnabled
                    | Qt::ItemIsSelectable
                    | Qt::ItemIsUserCheckable
                    | Qt::ItemIsEditable;

        Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsSelectable;
        
        switch(m_type) {
        case Qn::ResourceNode:
            if(m_model->context()->menu()->canTrigger(Qn::RenameAction, QnActionParameters(m_resource)))
                result |= Qt::ItemIsEditable;
            /* Fall through. */
        case Qn::ItemNode:
            if(m_flags & (QnResource::media | QnResource::layout | QnResource::server | QnResource::user))
                result |= Qt::ItemIsDragEnabled;
            break;
        case Qn::RecorderNode:
            result |= Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
            break;
        default:
            break;
        }
        return result;
    }

    QVariant data(int role, int column) const {
        switch(role) {
        case Qt::DisplayRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
            if (column == Qn::NameColumn)
                return m_displayName + (m_modified ? QLatin1String("*") : QString());
            break;
        case Qt::ToolTipRole:
            return m_displayName;
        case Qt::DecorationRole:
            if (column == Qn::NameColumn)
                return m_icon;
            break;
        case Qt::EditRole:
            if (column == Qn::NameColumn)
                return m_name;
            break;
        case Qt::CheckStateRole:
            if (column == Qn::CheckColumn)
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
            if(m_type == Qn::UsersNode) {
                return Qn::MainWindow_Tree_Users_Help;
            } else if(m_type == Qn::LocalNode) {
                return Qn::MainWindow_Tree_Local_Help;
            } else if(m_type == Qn::RecorderNode) {
                return Qn::MainWindow_Tree_Recorder_Help;
            } else if(m_flags & QnResource::layout) {
                if(m_model->context()->snapshotManager()->isFile(m_resource.dynamicCast<QnLayoutResource>())) {
                    return Qn::MainWindow_Tree_MultiVideo_Help;
                } else {
                    return Qn::MainWindow_Tree_Layouts_Help;
                }
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
        if (column == Qn::CheckColumn && role == Qt::CheckStateRole){
            m_checked = (Qt::CheckState)value.toInt();
            changeInternal();
            return true;
        }

        if(role != Qt::EditRole)
            return false;

        QnActionParameters parameters;
        if (m_type == Qn::RecorderNode) {
            //sending first camera to get groupId and check WriteName permission
            if (this->children().isEmpty())
                return false;
            Node* child = this->child(0);
            if (!child->resource())
                return false;
            parameters = QnActionParameters(child->resource()).withArgument(Qn::ResourceNameRole, value.toString());
        }
        else
            parameters = QnActionParameters(m_resource).withArgument(Qn::ResourceNameRole, value.toString());
        parameters.setArgument(Qn::NodeTypeRole, static_cast<int>(m_type));

        m_model->context()->menu()->trigger(Qn::RenameAction, parameters);
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

    // TODO: #GDM
    // This is a node construction method, so it does not really belong here.
    // See other node construction methods, QnResourcePoolModel::node(...).
    Node *recorder(const QString &groupId, const QString &groupName) {
        if (m_recorders.contains(groupId))
            return m_recorders[groupId];

        Node* recorder = new Node(m_model, Qn::RecorderNode, groupName);
        recorder->setParent(this);
        m_recorders[groupId] = recorder;
        return recorder;
    }

protected:
    void removeChildInternal(Node *child) {
        assert(child->parent() == this);

        if(isValid() && !isBastard()) {
            QModelIndex index = this->index(Qn::NameColumn);
            int row = m_children.indexOf(child);

            m_model->beginRemoveRows(index, row, row);
            m_children.removeOne(child);
            m_model->endRemoveRows();
        } else {
            m_children.removeOne(child);
        }
        if (this->type() == Qn::RecorderNode && m_children.size() == 0)
            setBastard(true);
    }

    void addChildInternal(Node *child) {
        assert(child->parent() == this);

        if(isValid() && !isBastard()) {
            QModelIndex index = this->index(Qn::NameColumn);
            int row = m_children.size();

            m_model->beginInsertRows(index, row, row);
            m_children.push_back(child);
            m_model->endInsertRows();
        } else {
            m_children.push_back(child);
        }
        if (this->type() == Qn::RecorderNode && m_children.size() > 0)
            setBastard(false);
    }

    void changeInternal() {
        if(!isValid() || isBastard())
            return;
        
        QModelIndex index = this->index(Qn::NameColumn);
        emit m_model->dataChanged(index, index.sibling(index.row(), Qn::ColumnCount - 1));
    }

private:
    //TODO: #GDM need complete recorder nodes structure refactor to get rid of this shit
    friend class QnResourcePoolModel;

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

    /** Recorder children of this node by group id. */
    QHash<QString, Node *> m_recorders;

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
QnResourcePoolModel::QnResourcePoolModel(Qn::NodeType rootNodeType, bool isFlat, QObject *parent):
    QAbstractItemModel(parent), 
    QnWorkbenchContextAware(parent),
    m_urlsShown(true),
    m_rootNodeType(rootNodeType),
    m_flat(isFlat)
{
    m_rootNodeTypes << Qn::LocalNode << Qn::UsersNode << Qn::ServersNode << Qn::RootNode << Qn::BastardNode;

    /* Create top-level nodes. */
    foreach(Qn::NodeType t, m_rootNodeTypes)
        m_rootNodes[t] = new Node(this, t);

    Qn::NodeType parentNodeType = rootNodeType == Qn::RootNode ? Qn::RootNode : Qn::BastardNode;
    foreach(Qn::NodeType t, m_rootNodeTypes)
        if (t != rootNodeType && t != parentNodeType)
            m_rootNodes[t]->setParent(m_rootNodes[parentNodeType]);

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
    foreach(Qn::NodeType t, m_rootNodeTypes)
        delete m_rootNodes[t];
}

QnResourcePtr QnResourcePoolModel::resource(const QModelIndex &index) const {
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

QnResourcePoolModel::Node *QnResourcePoolModel::node(const QnResourcePtr &resource) {
    QnResource *index = resource.data();
    if(!index)
        return m_rootNodes[m_rootNodeType];

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
        return m_rootNodes[m_rootNodeType];

    return static_cast<Node *>(index.internalPointer());
}

void QnResourcePoolModel::deleteNode(Node *node) {
    assert(node->type() == Qn::ResourceNode ||
           node->type() == Qn::ItemNode ||
           node->type() == Qn::RecorderNode);

    // TODO: #Elric implement this in Node's destructor.

    foreach(Node *childNode, node->children())
        deleteNode(childNode);

    switch(node->type()) {
    case Qn::ResourceNode:
        m_resourceNodeByResource.remove(node->resource().data());
        break;
    case Qn::ItemNode:
        m_itemNodeByUuid.remove(node->uuid());
        m_itemNodesByResource[node->resource().data()].removeAll(node);
        break;
    default:
        break;
    }

    delete node;
}

QnResourcePoolModel::Node *QnResourcePoolModel::expectedParent(Node *node) {
    assert(node->type() == Qn::ResourceNode);

    if(!node->resource())
        return m_rootNodes[m_rootNodeType];

    if(node->resourceFlags() & QnResource::user) {
        if(!accessController()->hasGlobalPermissions(Qn::GlobalEditUsersPermission)) {
            return m_rootNodes[m_rootNodeType];
        } else {
            return m_rootNodes[Qn::UsersNode];
        }
    }

    if(node->resourceFlags() & QnResource::server)
        return m_rootNodes[Qn::ServersNode];

    QnResourcePtr parentResource = resourcePool()->getResourceById(node->resource()->getParentId());
    if(!parentResource || (parentResource->flags() & QnResource::local_server) == QnResource::local_server) {
        if(node->resourceFlags() & QnResource::local) {
            return m_rootNodes[Qn::LocalNode];
        } else {
            return NULL;
        }
    } else {
        if (m_flat)
            return m_rootNodes[Qn::BastardNode];

        Node* parent = this->node(parentResource);

        if (QnSecurityCamResourcePtr camera = node->resource().dynamicCast<QnSecurityCamResource>()) {
            QString groupName = camera->getGroupName();
            QString groupId = camera->getGroupId();
            if(!groupId.isEmpty())
                parent = parent->recorder(groupId, groupName.isEmpty() ? groupId : groupName);
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
    return result;
}

QMimeData *QnResourcePoolModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeData = base_type::mimeData(indexes);
    if (mimeData) {
        const QStringList types = mimeTypes();

        bool pureTreeResourcesOnly = true;
        if (intersects(types, QnWorkbenchResource::resourceMimeTypes())) {
            QnResourceList resources;
            foreach (const QModelIndex &index, indexes) {
                Node *node = this->node(index);

                if (node && node->type() == Qn::RecorderNode) {
                    foreach (Node* child, node->children()) {
                        if (child->resource() && !resources.contains(child->resource()))
                            resources.append(child->resource());
                    }
                }

                if(node && node->resource() && !resources.contains(node->resource()))
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
        return base_type::dropMimeData(mimeData, action, row, column, parent);

    /* Decode. */
    QnResourceList resources = QnWorkbenchResource::deserializeResources(mimeData);

    /* Check where we're dropping it. */
    Node *node = this->node(parent);
    
    if(node->type() == Qn::ItemNode)
        node = node->parent(); /* Dropping into an item is the same as dropping into a layout. */

    if(node->parent() && (node->parent()->resourceFlags() & QnResource::server))
        node = node->parent(); /* Dropping into a server item is the same as dropping into a server */

    if(QnLayoutResourcePtr layout = node->resource().dynamicCast<QnLayoutResource>()) {
        QnResourceList medias;   // = resources.filtered<QnMediaResource>();
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
    assert(resource && resource->getId().isValid());

    connect(resource.data(), SIGNAL(parentIdChanged(const QnResourcePtr &)),                this, SLOT(at_resource_parentIdChanged(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(resourceChanged(const QnResourcePtr &)),                this, SLOT(at_resource_parentIdChanged(const QnResourcePtr &)));

    connect(resource.data(), SIGNAL(nameChanged(const QnResourcePtr &)),                    this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(statusChanged(const QnResourcePtr &)),                  this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(disabledChanged(const QnResourcePtr &)),                this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(urlChanged(const QnResourcePtr &)),                     this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(resourceChanged(const QnResourcePtr &)),                this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));

    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    if(layout) {
        connect(layout.data(), SIGNAL(itemAdded(const QnLayoutResourcePtr &, const QnLayoutItemData &)),    this, SLOT(at_resource_itemAdded(const QnLayoutResourcePtr &, const QnLayoutItemData &)));
        connect(layout.data(), SIGNAL(itemRemoved(const QnLayoutResourcePtr &, const QnLayoutItemData &)),  this, SLOT(at_resource_itemRemoved(const QnLayoutResourcePtr &, const QnLayoutItemData &)));
    }

    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(camera) {
        connect(camera.data(), &QnVirtualCameraResource::groupNameChanged, this, &QnResourcePoolModel::at_camera_groupNameChanged);
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

    deleteNode(node(resource));
}

void QnResourcePoolModel::at_context_userChanged() {
    m_rootNodes[Qn::LocalNode]->update();
    m_rootNodes[Qn::ServersNode]->update();
    m_rootNodes[Qn::UsersNode]->update();

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

void QnResourcePoolModel::at_resource_resourceChanged(const QnResourcePtr &resource) {
    node(resource)->update();

    foreach(Node *node, m_itemNodesByResource[resource.data()])
        node->update();
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

void QnResourcePoolModel::at_resource_itemRemoved(const QnLayoutResourcePtr &, const QnLayoutItemData &item) {
    deleteNode(node(item.uuid));
}

//TODO: #GDM need complete recorder nodes structure refactor to get rid of this shit
void QnResourcePoolModel::at_camera_groupNameChanged(const QnSecurityCamResourcePtr &camera) {
    const QString groupId = camera->getGroupId();
    foreach (Node* node, m_resourceNodeByResource) {
        if (!node->m_recorders.contains(groupId))
            continue;
        node->m_recorders[groupId]->m_name = camera->getGroupName();
        node->m_recorders[groupId]->m_displayName = camera->getGroupName();
        node->changeInternal();
    }
}

