#include "resource_pool_model_node.h"

#include <common/common_meta_types.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>

#include <ui/actions/action_manager.h>
#include <ui/common/ui_resource_name.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_pool_model.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

QnResourcePoolModelNode::QnResourcePoolModelNode(QnResourcePoolModel *model, Qn::NodeType type, const QString &name):
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

    m_editable.checked = false;
}

/**
 * Constructor for resource nodes.
 */
QnResourcePoolModelNode::QnResourcePoolModelNode(QnResourcePoolModel *model, const QnResourcePtr &resource, Qn::NodeType nodeType):
    m_model(model),
    m_type(nodeType),
    m_state(Invalid),
    m_bastard(false),
    m_parent(NULL),
    m_status(QnResource::Offline),
    m_modified(false),
    m_checked(Qt::Unchecked)
{
    assert(model != NULL);
    assert(nodeType == Qn::ResourceNode ||
           nodeType == Qn::EdgeNode);

    setResource(resource);

    m_editable.checked = false;
}

/**
 * Constructor for item nodes.
 */
QnResourcePoolModelNode::QnResourcePoolModelNode(QnResourcePoolModel *model, const QUuid &uuid, Qn::NodeType nodeType):
    m_model(model),
    m_type(nodeType),
    m_uuid(uuid),
    m_state(Invalid),
    m_bastard(false),
    m_parent(NULL),
    m_status(QnResource::Offline),
    m_modified(false),
    m_checked(Qt::Unchecked)
{
    assert(model != NULL);

    m_editable.checked = false;
}

QnResourcePoolModelNode::~QnResourcePoolModelNode() {
    clear();

    foreach(QnResourcePoolModelNode *childNode, m_children) {
        childNode->setParent(NULL);
        m_model->deleteNode(childNode);
    }
}

void QnResourcePoolModelNode::clear() {
    setParent(NULL);

    if (m_type == Qn::ItemNode ||
        m_type == Qn::ResourceNode || 
        m_type == Qn::VideoWallItemNode || 
        m_type == Qn::EdgeNode)
        setResource(QnResourcePtr());
}

void QnResourcePoolModelNode::setResource(const QnResourcePtr &resource) {
    assert(
        m_type == Qn::ItemNode ||
        m_type == Qn::ResourceNode || 
        m_type == Qn::VideoWallItemNode || 
        m_type == Qn::EdgeNode);

    if(m_resource == resource)
        return;

    if(m_resource && (m_type == Qn::ItemNode || m_type == Qn::VideoWallItemNode))
        m_model->m_itemNodesByResource[m_resource].removeOne(this);

    m_resource = resource;

    if(m_resource && (m_type == Qn::ItemNode || m_type == Qn::VideoWallItemNode))
        m_model->m_itemNodesByResource[m_resource].push_back(this);

    update();
}

void QnResourcePoolModelNode::update() {
    /* Update stored fields. */
    if(m_type == Qn::ResourceNode || m_type == Qn::ItemNode || m_type == Qn::EdgeNode) {
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
            m_icon = qnResIconCache->icon(m_resource);
            m_displayName = getResourceName(m_resource);
        }
    } else if (m_type == Qn::VideoWallItemNode) {
        m_searchString = QString();
        m_flags = 0;
        m_status = QnResource::Offline;
        m_icon = qnResIconCache->icon(QnResourceIconCache::VideoWallItem | QnResourceIconCache::Offline);

        QnVideoWallItemIndex index = qnResPool->getVideoWallItemByUuid(m_uuid);
        if (!index.isNull()) {
            QnVideoWallItem item = index.item();

            if (item.online) {
                m_status = QnResource::Online;
                m_icon = qnResIconCache->icon(QnResourceIconCache::VideoWallItem);              
            }

            if(m_resource.isNull()) {
                m_displayName = m_name = item.name;
            } else {
                m_name = item.name;
                QString resourceName = m_resource->getName();
                if (m_flags & QnResource::desktop_camera)
                    resourceName = tr("%1's Screen", "%1 means user's name").arg(resourceName);
                m_displayName = QString(QLatin1String("%1 <%2>")).arg(item.name).arg(resourceName);
            }
        } else {
            m_displayName = m_name = QString();
        }
    } else if (m_type == Qn::VideoWallMatrixNode) {
        m_status = QnResource::Online;
        m_searchString = QString();
        m_flags = 0; 
        m_icon = qnResIconCache->icon(QnResourceIconCache::VideoWallMatrix);
        foreach (const QnVideoWallResourcePtr &videowall, qnResPool->getResources().filtered<QnVideoWallResource>()) {
            if (!videowall->matrices()->hasItem(m_uuid))
                continue;
            m_displayName = m_name = videowall->matrices()->getItem(m_uuid).name;
            break;
        }
    }

    /* Update bastard state. */
    bool bastard = false;
    switch(m_type) {
    case Qn::ItemNode:
        bastard = m_resource.isNull();
        break;
    case Qn::VideoWallItemNode:
    case Qn::VideoWallMatrixNode:
        bastard = false;
        break;
    case Qn::ResourceNode:
        bastard = !(m_model->accessController()->permissions(m_resource) & Qn::ReadPermission); /* Hide non-readable resources. */
        if(!bastard)
            if(QnLayoutResourcePtr layout = m_resource.dynamicCast<QnLayoutResource>()) {
                /* Hide local layouts that are not file-based. */ 
                bastard = m_model->snapshotManager()->isLocal(layout) && !m_model->snapshotManager()->isFile(layout);

                /* Hide "Preview Search" layouts */
                bastard |= layout->data().contains(Qn::LayoutSearchStateRole);
            }
        if(!bastard)
            bastard = (m_flags & QnResource::desktop_camera) == QnResource::desktop_camera; /* Hide desktop camera resources from the tree. */
        if(!bastard)
            bastard = (m_flags & QnResource::local_server) == QnResource::local_server; /* Hide local server resource. */
        if(!bastard)
            bastard = (m_flags & QnResource::local_media) == QnResource::local_media && m_resource->getUrl().startsWith(QLatin1String("layout://")); //TODO: #Elric hack hack hack
        if (!bastard)
            bastard = QnMediaServerResource::isEdgeServer(m_resource);
        break;
    case Qn::EdgeNode:
        bastard = !(m_model->accessController()->permissions(m_resource) & Qn::ReadPermission); /* Hide non-readable resources. */
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

void QnResourcePoolModelNode::updateRecursive() {
    update();

    foreach(QnResourcePoolModelNode *child, m_children)
        child->updateRecursive();
}

Qn::NodeType QnResourcePoolModelNode::type() const {
    return m_type;
}

const QnResourcePtr& QnResourcePoolModelNode::resource() const {
    return m_resource;
}

QnResource::Flags QnResourcePoolModelNode::resourceFlags() const {
    return m_flags;
}

QnResource::Status QnResourcePoolModelNode::resourceStatus() const {
    return m_status;
}

const QUuid& QnResourcePoolModelNode::uuid() const {
    return m_uuid;
}

QnResourcePoolModelNode::State QnResourcePoolModelNode::state() const {
    return m_state;
}

bool QnResourcePoolModelNode::isValid() const {
    return m_state == Normal;
}

void QnResourcePoolModelNode::setState(State state) {
    if(m_state == state)
        return;

    m_state = state;

    foreach(QnResourcePoolModelNode *node, m_children)
        node->setState(state);
}

bool QnResourcePoolModelNode::isBastard() const {
    return m_bastard;
}

void QnResourcePoolModelNode::setBastard(bool bastard) {
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

const QList<QnResourcePoolModelNode *>& QnResourcePoolModelNode::children() const {
    return m_children;
}

QnResourcePoolModelNode* QnResourcePoolModelNode::child(int index) {
    return m_children[index];
}

QnResourcePoolModelNode* QnResourcePoolModelNode::parent() const {
    return m_parent;
}

void QnResourcePoolModelNode::setParent(QnResourcePoolModelNode *parent) {
    if(m_parent == parent)
        return;

    if(m_parent && !m_bastard)
        m_parent->removeChildInternal(this);

    m_parent = parent;

    if(m_parent) {
        if(!m_bastard) {
            setState(m_parent->state());
            m_parent->addChildInternal(this);

            if (m_type == Qn::VideoWallItemNode)
                update();
        }
    } else {
        setState(Invalid);
    }
}

QModelIndex QnResourcePoolModelNode::index(int col) {
    assert(isValid()); /* Only valid nodes have indices. */

    if(m_parent == NULL)
        return QModelIndex(); /* That's root node. */

    return index(m_parent->m_children.indexOf(this), col);
}

QModelIndex QnResourcePoolModelNode::index(int row, int col) {
    assert(isValid()); /* Only valid nodes have indices. */
    assert(m_parent != NULL && row == m_parent->m_children.indexOf(this));

    return m_model->createIndex(row, col, this);
}

Qt::ItemFlags QnResourcePoolModelNode::flags(int column) const {
    if (column == Qn::CheckColumn)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;

    Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsSelectable;

    if (!m_editable.checked) {
        switch(m_type) {
        case Qn::ResourceNode:
        case Qn::EdgeNode:
            m_editable.value = m_model->context()->menu()->canTrigger(Qn::RenameAction, QnActionParameters(m_resource)); //TODO: #GDM #VW make this context-aware?
            break;
        case Qn::VideoWallItemNode:
        case Qn::VideoWallMatrixNode:
            m_editable.value = (m_model->context()->accessController()->globalPermissions() & Qn::GlobalEditVideoWallPermission);   //TODO: #GDM #VW make this context-aware?
            break;
        case Qn::RecorderNode:
            m_editable.value = true;
            break;
        default:
            m_editable.value = false;
            break;
        }
        m_editable.checked = true;
    }

    if(m_editable.value)
        result |= Qt::ItemIsEditable;

    switch(m_type) {
    case Qn::ResourceNode:
    case Qn::EdgeNode:
    case Qn::ItemNode:
        if(m_flags & (QnResource::media | QnResource::layout | QnResource::server | QnResource::user | QnResource::videowall))
            result |= Qt::ItemIsDragEnabled;
        break;
    case Qn::VideoWallItemNode: //TODO: #GDM #VW drag of empty item on scene should create new layout
    case Qn::RecorderNode:
        result |= Qt::ItemIsDragEnabled; 
        break;
    default:
        break;
    }
    return result;
}

QVariant QnResourcePoolModelNode::data(int role, int column) const {
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
        if (
            m_type == Qn::ItemNode 
            || m_type == Qn::VideoWallItemNode 
            || m_type == Qn::VideoWallMatrixNode
            )
            return QVariant::fromValue<QUuid>(m_uuid);
        break;
    case Qn::ResourceSearchStringRole:
        return m_searchString;
    case Qn::ResourceStatusRole:
        return QVariant::fromValue<int>(m_status);
    case Qn::NodeTypeRole:
        return qVariantFromValue(m_type);
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

bool QnResourcePoolModelNode::setData(const QVariant &value, int role, int column) {
    if (column == Qn::CheckColumn && role == Qt::CheckStateRole){
        m_checked = (Qt::CheckState)value.toInt();
        changeInternal();
        return true;
    }

    if(role != Qt::EditRole)
        return false;

    if (value.toString().isEmpty())
        return false;

    QnActionParameters parameters;
    if (m_type == Qn::VideoWallItemNode) {
        QnVideoWallItemIndex index = qnResPool->getVideoWallItemByUuid(m_uuid);
        if (index.isNull())
            return false;
        parameters = QnActionParameters(QnVideoWallItemIndexList() << index);
    } else if (m_type == Qn::VideoWallMatrixNode) {
        QnVideoWallMatrixIndex index = qnResPool->getVideoWallMatrixByUuid(m_uuid);
        if (index.isNull())
            return false;
        parameters = QnActionParameters(QnVideoWallMatrixIndexList() << index);
    } else if (m_type == Qn::RecorderNode) {
        //sending first camera to get groupId and check WriteName permission
        if (this->children().isEmpty())
            return false;
        QnResourcePoolModelNode* child = this->child(0);
        if (!child->resource())
            return false;
        parameters = QnActionParameters(child->resource());
    } else {
        parameters = QnActionParameters(m_resource);
    }
    parameters.setArgument(Qn::ResourceNameRole, value.toString());
    parameters.setArgument(Qn::NodeTypeRole, m_type);

    m_model->context()->menu()->trigger(Qn::RenameAction, parameters);
    return true;
}

bool QnResourcePoolModelNode::isModified() const {
    return !m_modified;
}

void QnResourcePoolModelNode::setModified(bool modified) {
    if(m_modified == modified)
        return;

    m_modified = modified;

    changeInternal();
}

void QnResourcePoolModelNode::removeChildInternal(QnResourcePoolModelNode *child) {
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

void QnResourcePoolModelNode::addChildInternal(QnResourcePoolModelNode *child) {
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

void QnResourcePoolModelNode::changeInternal() {
    if(!isValid() || isBastard())
        return;

    QModelIndex index = this->index(Qn::NameColumn);
    emit m_model->dataChanged(index, index.sibling(index.row(), Qn::ColumnCount - 1));
}
