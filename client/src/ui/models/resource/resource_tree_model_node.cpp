#include "resource_tree_model_node.h"

#include <common/common_meta_types.h>
#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>

#include <api/global_settings.h>

#include <ui/actions/action_manager.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

namespace
{

    /* Set of node types, that are require children to be visible. */
    bool nodeRequiresChildren(Qn::NodeType nodeType)
    {
        static QSet<Qn::NodeType> result;
        if (result.isEmpty())
            result
            << Qn::OtherSystemsNode
            << Qn::WebPagesNode
            << Qn::ServersNode
            << Qn::UserDevicesNode
            << Qn::RecorderNode
            << Qn::SystemNode;
        return result.contains(nodeType);
    }


}

QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, Qn::NodeType nodeType, const QnUuid& uuid) :
    base_type(),
    QnWorkbenchContextAware(model),
    m_model(model),
    m_type(nodeType),
    m_uuid(uuid),
    m_state(Normal),
    m_bastard(false),
    m_parent(NULL),
    m_status(Qn::Online),
    m_modified(false),
    m_checkState(Qt::Unchecked)
{
    NX_ASSERT(model != NULL);
    m_editable.checked = false;
}



QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, Qn::NodeType nodeType):
    QnResourceTreeModelNode(model, nodeType, QnUuid())
{
    switch(nodeType)
    {
    case Qn::RootNode:
        m_displayName = m_name = lit("Root");   /* This node is not visible directly. */
        break;
    case Qn::BastardNode:
        m_displayName = m_name = lit("Invalid Resources");
        m_bastard = true; /* This node is always hidden. */
        m_state = Invalid;
        break;
    case Qn::LocalResourcesNode:
        m_displayName = m_name = tr("Local");
        m_icon = qnResIconCache->icon(QnResourceIconCache::LocalResources);
        break;
    case Qn::CurrentSystemNode:
        m_icon = qnResIconCache->icon(QnResourceIconCache::CurrentSystem);
        break;
    case Qn::SeparatorNode:
    case Qn::LocalSeparatorNode:
        m_displayName = QString();
        m_name = lit("-");
        break;
    case Qn::ServersNode:
        m_displayName = m_name = tr("Servers");
        m_icon = qnResIconCache->icon(QnResourceIconCache::Servers);
        break;
    case Qn::OtherSystemsNode:
        m_displayName = m_name = tr("Other Systems");
        m_icon = qnResIconCache->icon(QnResourceIconCache::OtherSystems);
        break;
    case Qn::UsersNode:
        m_displayName = m_name = tr("Users");
        m_icon = qnResIconCache->icon(QnResourceIconCache::Users);
        break;
    case Qn::WebPagesNode:
        m_displayName = m_name = tr("Web Pages");
        m_icon = qnResIconCache->icon(QnResourceIconCache::WebPages);
        break;
    case Qn::UserDevicesNode:
        m_displayName = m_name = QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Devices"),
            tr("Cameras")
        );
        m_icon = qnResIconCache->icon(QnResourceIconCache::Cameras);
        break;
    case Qn::LayoutsNode:
        m_displayName = m_name = tr("Layouts");
        m_icon = qnResIconCache->icon(QnResourceIconCache::Layouts);
        break;
    case Qn::RecorderNode:
        m_icon = qnResIconCache->icon(QnResourceIconCache::Recorder);
        break;
    case Qn::SystemNode:
        m_icon = qnResIconCache->icon(QnResourceIconCache::OtherSystem);
        break;
    default:
        break;
    }

    /* Invisible by default until has children. */
    m_bastard = nodeRequiresChildren(nodeType);
}

/**
* Constructor for other virtual group nodes (recorders, other systems).
*/
QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, Qn::NodeType nodeType, const QString &name) :
    QnResourceTreeModelNode(model, nodeType)
{
    NX_ASSERT(nodeType == Qn::SystemNode || nodeType == Qn::RecorderNode);
    m_displayName = m_name = name;
    m_state = Invalid;
}

/**
 * Constructor for resource nodes.
 */
QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, const QnResourcePtr &resource, Qn::NodeType nodeType):
    QnResourceTreeModelNode(model, nodeType)
{
    NX_ASSERT(nodeType == Qn::ResourceNode || nodeType == Qn::EdgeNode || nodeType == Qn::SharedLayoutNode);
    m_state = Invalid;
    m_status = Qn::Offline;
    setResource(resource);
}

/**
 * Constructor for item nodes.
 */
QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, const QnUuid &uuid, Qn::NodeType nodeType):
    QnResourceTreeModelNode(model, nodeType, uuid)
{
    NX_ASSERT(nodeType == Qn::LayoutItemNode || nodeType == Qn::VideoWallItemNode || nodeType == Qn::VideoWallMatrixNode);
    m_state = Invalid;
    m_status = Qn::Offline;
}

QnResourceTreeModelNode::~QnResourceTreeModelNode()
{}

void QnResourceTreeModelNode::setResource(const QnResourcePtr& resource)
{
    NX_ASSERT(
        m_type == Qn::LayoutItemNode ||
        m_type == Qn::ResourceNode ||
        m_type == Qn::VideoWallItemNode ||
        m_type == Qn::EdgeNode ||
        m_type == Qn::SharedLayoutNode
    );

    if (m_resource == resource)
        return;

    m_resource = resource;
    update();
}

void QnResourceTreeModelNode::update()
{
    /* Update stored fields. */
    if (m_type == Qn::ResourceNode ||
        m_type == Qn::LayoutItemNode ||
        m_type == Qn::EdgeNode ||
        m_type == Qn::SharedLayoutNode)
    {
        if(m_resource.isNull())
        {
            m_displayName = m_name = QString();
            m_flags = 0;
            m_status = Qn::Online;
            m_searchString = QString();
            m_icon = QIcon();
        }
        else
        {
            m_name = m_resource->getName();
            m_flags = m_resource->flags();
            m_status = m_resource->getStatus();
            m_searchString = m_resource->toSearchString();
            m_icon = qnResIconCache->icon(m_resource);
            m_displayName = QnResourceDisplayInfo(m_resource).toString(Qn::RI_NameOnly);
        }
    }
    else if (m_type == Qn::VideoWallItemNode)
    {
        m_searchString = QString();
        m_flags = 0;
        m_status = Qn::Offline;
        m_icon = qnResIconCache->icon(QnResourceIconCache::VideoWallItem | QnResourceIconCache::Offline);

        QnVideoWallItemIndex index = qnResPool->getVideoWallItemByUuid(m_uuid);
        if (!index.isNull())
        {
            QnVideoWallItem item = index.item();

            if (item.runtimeStatus.online)
            {
                if (item.runtimeStatus.controlledBy.isNull())
                {
                    m_status = Qn::Online;
                    m_icon = qnResIconCache->icon(QnResourceIconCache::VideoWallItem);
                }
                else if (item.runtimeStatus.controlledBy == qnCommon->moduleGUID())
                {
                    m_status = Qn::Online;
                    m_icon = qnResIconCache->icon(QnResourceIconCache::VideoWallItem | QnResourceIconCache::Control);
                }
                else
                {
                    m_status = Qn::Unauthorized;
                    m_icon = qnResIconCache->icon(QnResourceIconCache::VideoWallItem | QnResourceIconCache::Locked);
                }
            }

            m_displayName = m_name = item.name;
        }
        else
        {
            m_displayName = m_name = QString();
        }
    }
    else if (m_type == Qn::VideoWallMatrixNode)
    {
        m_status = Qn::Online;
        m_searchString = QString();
        m_flags = 0;
        m_icon = qnResIconCache->icon(QnResourceIconCache::VideoWallMatrix);
        for (const QnVideoWallResourcePtr &videowall: qnResPool->getResources<QnVideoWallResource>())
        {
            if (!videowall->matrices()->hasItem(m_uuid))
                continue;
            m_displayName = m_name = videowall->matrices()->getItem(m_uuid).name;
            break;
        }
    }
    else if (m_type == Qn::CurrentSystemNode)
    {
        m_displayName = qnCommon->localSystemName();
    }
    else if (m_type == Qn::RecorderNode)
    {

        //TODO: #GDM
    }

    /* Update bastard state. */
    setBastard(calculateBastard());

    /* Notify views. */
    changeInternal();
}

void QnResourceTreeModelNode::updateRecursive()
{
    update();

    for(auto child: m_children)
        child->updateRecursive();
}

Qn::NodeType QnResourceTreeModelNode::type() const
{
    return m_type;
}

QnResourcePtr QnResourceTreeModelNode::resource() const
{
    return m_resource;
}

Qn::ResourceFlags QnResourceTreeModelNode::resourceFlags() const
{
    return m_flags;
}

const QnUuid& QnResourceTreeModelNode::uuid() const
{
    return m_uuid;
}

QnResourceTreeModelNode::State QnResourceTreeModelNode::state() const
{
    return m_state;
}

bool QnResourceTreeModelNode::isValid() const
{
    return m_state == Normal;
}

void QnResourceTreeModelNode::setState(State state)
{
    if (m_state == state)
        return;

    m_state = state;

    for (auto child: m_children)
        child->setState(state);
}

bool QnResourceTreeModelNode::calculateBastard() const
{
    if (nodeRequiresChildren(m_type) && m_children.isEmpty())
        return true;

    /* Here we can narrow nodes visibility, based on permissions, if needed. */
    bool isLoggedIn = !context()->user().isNull();
    bool isAdmin = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);

    switch (m_type)
    {
    /* Hide non-readable resources. */
    case Qn::LayoutItemNode:
        /* Hide resource nodes without resource. */
        if (!m_resource)
            return true;

        /* Only admin can see items under shared layout links. */
        if (m_parent && m_parent->type() == Qn::SharedLayoutNode)
            return !isAdmin;

        return !accessController()->hasPermissions(m_resource, Qn::ReadPermission);

    /* These will be hidden or displayed together with videowall. */
    case Qn::VideoWallItemNode:
    case Qn::VideoWallMatrixNode:
        return false;

    /* Always hidden. */
    case Qn::BastardNode:
        return true;

    case Qn::OtherSystemsNode:
        return !isAdmin || !QnGlobalSettings::instance()->isServerAutoDiscoveryEnabled();

    case Qn::UsersNode:
    case Qn::ServersNode:
        return !isAdmin;

    case Qn::UserDevicesNode:
        return !isLoggedIn || isAdmin;

    case Qn::LayoutsNode:
        return !isLoggedIn;

    case Qn::EdgeNode:
        /* Hide resource nodes without resource. */
        if (!m_resource)
            return true;

        /* Only admins can see edge nodes. */
        return !isAdmin;

    case Qn::SharedLayoutNode:
        /* Hide resource nodes without resource. */
        if (!m_resource)
            return true;

        /* Only admins can see shared layout links. */
        return !isAdmin;

    case Qn::ResourceNode:
        /* Hide resource nodes without resource. */
        if (!m_resource)
            return true;

        /* Hide non-readable resources. */
        if (!accessController()->hasPermissions(m_resource, Qn::ReadPermission))
            return true;

        if (QnLayoutResourcePtr layout = m_resource.dynamicCast<QnLayoutResource>())
        {
            /* Hide local layouts that are not file-based. */
            if (snapshotManager()->isLocal(layout) && !layout->isFile())
                return true;

            /* Hide "Preview Search" layouts */
            if (layout->data().contains(Qn::LayoutSearchStateRole)) //TODO: #GDM make it consistent with QnWorkbenchLayout::isSearchLayout
                return true;

            if (qnResPool->isAutoGeneratedLayout(layout))
                return true;

            return false;
        }

        if (QnUserResourcePtr user = m_resource.dynamicCast<QnUserResource>())
        {
            /* Hide disabled users. */
            return !user->isEnabled();
        }

        {
#ifndef DESKTOP_CAMERA_DEBUG
            /* Hide desktop camera resources from the tree. */
            if (m_flags.testFlag(Qn::desktop_camera))
                return true;
#endif
            /* Hide local server resource. */
            if (m_flags.testFlag(Qn::local_server))
                return true;

            //TODO: #Elric hack hack hack VMS-1725
            if (m_flags.testFlag(Qn::local_media) && m_resource->getUrl().startsWith(lit("layout://")))
                return true;

            /* Hide storages. */
            if (m_resource.dynamicCast<QnStorageResource>())
                return true;

            /* Hide edge servers, camera will be displayed instead. */
            if (QnMediaServerResource::isHiddenServer(m_resource) &&
                !qnResPool->getResourcesByParentId(m_resource->getId()).filtered<QnVirtualCameraResource>().isEmpty())
            {
                return true;
            }
        }
        return false;

    default:
        NX_ASSERT("Should never get here");
        return false;
    }
}

bool QnResourceTreeModelNode::isBastard() const
{
    return m_bastard;
}

void QnResourceTreeModelNode::setBastard(bool bastard)
{
    if (m_bastard == bastard)
        return;

    m_bastard = bastard;

    if (!m_parent)
        return;
    setState(m_parent->state());

    if (m_bastard)
        m_parent->removeChildInternal(toSharedPointer());
    else
        m_parent->addChildInternal(toSharedPointer());
}

QList<QnResourceTreeModelNodePtr> QnResourceTreeModelNode::children() const
{
    return m_children;
}

QnResourceTreeModelNodePtr QnResourceTreeModelNode::child(int index)
{
    return m_children[index];
}

QnResourceTreeModelNodePtr QnResourceTreeModelNode::parent() const
{
    return m_parent;
}

void QnResourceTreeModelNode::setParent(const QnResourceTreeModelNodePtr& parent)
{
    if (m_parent == parent)
        return;

    if (m_parent && !m_bastard)
        m_parent->removeChildInternal(toSharedPointer());

    m_parent = parent;

    if (m_parent)
    {
        setState(m_parent->state());
        if (!m_bastard)
        {
            m_parent->addChildInternal(toSharedPointer());

            if (m_type == Qn::VideoWallItemNode)
                update();
        }
    }
    else
    {
        setState(Invalid);
    }
}

QModelIndex QnResourceTreeModelNode::createIndex(int row, int col)
{
    if (!m_model)
         return QModelIndex();

    NX_ASSERT(isValid()); /* Only valid nodes have indices. */
    NX_ASSERT(m_parent != NULL && row == m_parent->m_children.indexOf(toSharedPointer()));

    return m_model->createIndex(row, col, this);
}

QModelIndex QnResourceTreeModelNode::createIndex(int col)
{
    NX_ASSERT(isValid()); /* Only valid nodes have indices. */

    if (m_parent == NULL)
        return QModelIndex(); /* That's root node. */

    return createIndex(m_parent->m_children.indexOf(toSharedPointer()), col);
}

Qt::ItemFlags QnResourceTreeModelNode::flags(int column) const
{
    if (Qn::isSeparatorNode(m_type))
    {
        /* No Editable/Selectable flags. */
        return Qt::ItemNeverHasChildren;
    }

    if (column == Qn::CheckColumn)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;

    Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsSelectable;

    if (!m_editable.checked)
    {
        switch(m_type)
        {
        case Qn::ResourceNode:
        case Qn::EdgeNode:
            m_editable.value = menu()->canTrigger(QnActions::RenameResourceAction, QnActionParameters(m_resource));
            break;
        case Qn::VideoWallItemNode:
        case Qn::VideoWallMatrixNode:
            m_editable.value = accessController()->hasGlobalPermission(Qn::GlobalControlVideoWallPermission);
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

    switch(m_type)
    {
    case Qn::ResourceNode:
    case Qn::EdgeNode:
    case Qn::LayoutItemNode:
    case Qn::SharedLayoutNode:
        if(m_flags & (Qn::media | Qn::layout | Qn::server | Qn::user | Qn::videowall | Qn::web_page))
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

QVariant QnResourceTreeModelNode::data(int role, int column) const
{
    switch(role)
    {
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
            return m_checkState;
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
            m_type == Qn::LayoutItemNode
            || m_type == Qn::VideoWallItemNode
            || m_type == Qn::VideoWallMatrixNode
            )
            return QVariant::fromValue<QnUuid>(m_uuid);
        break;
    case Qn::ResourceSearchStringRole:
        return m_searchString;
    case Qn::ResourceStatusRole:
        return QVariant::fromValue<int>(m_status);
    case Qn::NodeTypeRole:
        return qVariantFromValue(m_type);

    case Qn::HelpTopicIdRole:
        if (m_type == Qn::UsersNode)
            return Qn::MainWindow_Tree_Users_Help;

        if (m_type == Qn::LocalResourcesNode)
            return Qn::MainWindow_Tree_Local_Help;

        if (m_type == Qn::RecorderNode)
            return Qn::MainWindow_Tree_Recorder_Help;

        if (m_type == Qn::VideoWallItemNode)
            return Qn::Videowall_Display_Help;

        if (m_type == Qn::VideoWallMatrixNode)
            return Qn::Videowall_Matrix_Help;

        if (m_flags.testFlag(Qn::layout))
        {
            if (QnLayoutResourcePtr layout = m_resource.dynamicCast<QnLayoutResource>())
                if (layout->isFile())
                    return Qn::MainWindow_Tree_MultiVideo_Help;
            return Qn::MainWindow_Tree_Layouts_Help;
        }

        if (m_flags.testFlag(Qn::user))
            return Qn::MainWindow_Tree_Users_Help;

        if (m_flags.testFlag(Qn::local))
        {
            if (m_flags.testFlag(Qn::video))
                return Qn::MainWindow_Tree_Exported_Help;
            return Qn::MainWindow_Tree_Local_Help;
        }

        if (m_flags.testFlag(Qn::server))
            return Qn::MainWindow_Tree_Servers_Help;

        if (m_flags.testFlag(Qn::io_module))
            return Qn::IOModules_Help;

        if (m_flags.testFlag(Qn::live_cam))
            return Qn::MainWindow_Tree_Camera_Help;

        if (m_flags.testFlag(Qn::videowall))
            return Qn::Videowall_Help;

        return -1;

    default:
        break;
    }

    return QVariant();
}

bool QnResourceTreeModelNode::setData(const QVariant &value, int role, int column)
{
    if (column == Qn::CheckColumn && role == Qt::CheckStateRole)
    {
        m_checkState = static_cast<Qt::CheckState>(value.toInt());
        changeInternal();
        return true;
    }

    if (role != Qt::EditRole)
        return false;

    if (value.toString().isEmpty())
        return false;

    QnActionParameters parameters;
    bool isVideoWallEntity = false;
    if (m_type == Qn::VideoWallItemNode)
    {
        QnVideoWallItemIndex index = qnResPool->getVideoWallItemByUuid(m_uuid);
        if (index.isNull())
            return false;
        parameters = QnActionParameters(QnVideoWallItemIndexList() << index);
        isVideoWallEntity = true;
    }
    else if (m_type == Qn::VideoWallMatrixNode)
    {
        QnVideoWallMatrixIndex index = qnResPool->getVideoWallMatrixByUuid(m_uuid);
        if (index.isNull())
            return false;
        parameters = QnActionParameters(QnVideoWallMatrixIndexList() << index);
        isVideoWallEntity = true;
    }
    else if (m_type == Qn::RecorderNode)
    {
        //sending first camera to get groupId and check WriteName permission
        if (children().isEmpty())
            return false;

        auto child = this->child(0);
        if (!child->resource())
            return false;
        parameters = QnActionParameters(child->resource());
    }
    else
    {
        parameters = QnActionParameters(m_resource);
    }
    parameters.setArgument(Qn::ResourceNameRole, value.toString());
    parameters.setArgument(Qn::NodeTypeRole, m_type);

    if (isVideoWallEntity)
        menu()->trigger(QnActions::RenameVideowallEntityAction, parameters);
    else
        menu()->trigger(QnActions::RenameResourceAction, parameters);
    return true;
}

bool QnResourceTreeModelNode::isModified() const
{
    return !m_modified;
}

void QnResourceTreeModelNode::setModified(bool modified)
{
    if(m_modified == modified)
        return;

    m_modified = modified;

    changeInternal();
}

void QnResourceTreeModelNode::removeChildInternal(const QnResourceTreeModelNodePtr& child)
{
    NX_ASSERT(child->parent() == this);

    if (isValid() && !isBastard() && m_model)
    {
        QModelIndex index = createIndex(Qn::NameColumn);
        int row = m_children.indexOf(child);

        m_model->beginRemoveRows(index, row, row);
        m_children.removeOne(child);
        m_model->endRemoveRows();
    }
    else
    {
        m_children.removeOne(child);
    }

    if (nodeRequiresChildren(m_type) && m_children.size() == 0)
        setBastard(true);
}

void QnResourceTreeModelNode::addChildInternal(const QnResourceTreeModelNodePtr& child)
{
    NX_ASSERT(child->parent() == this);

    if (isValid() && !isBastard() && m_model)
    {
        QModelIndex index = createIndex(Qn::NameColumn);
        int row = m_children.size();

        m_model->beginInsertRows(index, row, row);
        m_children.push_back(child);
        m_model->endInsertRows();
    }
    else
    {
        m_children.push_back(child);
    }

    /* Check if we must display parent node. */
    if (nodeRequiresChildren(m_type) && isBastard())
        setBastard(calculateBastard());
}

void QnResourceTreeModelNode::changeInternal()
{
    if (!isValid() || isBastard() || !m_model)
        return;

    QModelIndex index = createIndex(Qn::NameColumn);
    emit m_model->dataChanged(index, index.sibling(index.row(), Qn::ColumnCount - 1));
}
