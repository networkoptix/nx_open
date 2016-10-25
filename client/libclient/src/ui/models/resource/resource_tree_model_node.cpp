#include "resource_tree_model_node.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/resource_access_manager.h>

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

namespace {

/* Set of node types, that are require children to be visible. */
bool nodeRequiresChildren(Qn::NodeType nodeType)
{
    static QSet<Qn::NodeType> result;
    if (result.isEmpty())
        result
        << Qn::OtherSystemsNode
        << Qn::WebPagesNode
        << Qn::ServersNode
        << Qn::UserResourcesNode
        << Qn::RecorderNode
        << Qn::SystemNode
        << Qn::RoleUsersNode
        << Qn::LayoutsNode
        << Qn::SharedLayoutsNode
        << Qn::SharedResourcesNode
        << Qn::RoleUsersNode
        ;
    return result.contains(nodeType);
}

}

QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, Qn::NodeType nodeType, const QnUuid& uuid) :
    base_type(),
    QnWorkbenchContextAware(model),
    m_initialized(false),
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
    m_icon = calculateIcon();

    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged, this,
        &QnResourceTreeModelNode::handlePermissionsChanged);
}

QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, Qn::NodeType nodeType):
    QnResourceTreeModelNode(model, nodeType, QnUuid())
{
    switch(nodeType)
    {
    case Qn::RootNode:
        setName(lit("Root"));   /* This node is not visible directly. */
        break;
    case Qn::BastardNode:
        setName(lit("Invalid Resources"));
        m_bastard = true; /* This node is always hidden. */
        m_state = Invalid;
        break;
    case Qn::LocalResourcesNode:
        setName(tr("Local"));
        break;
    case Qn::CurrentSystemNode:
        break;
    case Qn::CurrentUserNode:
        m_flags = Qn::user;
        break;
    case Qn::SeparatorNode:
    case Qn::LocalSeparatorNode:
        m_displayName = QString();
        m_name = lit("-");
        break;
    case Qn::ServersNode:
        setName(tr("Servers"));
        break;
    case Qn::OtherSystemsNode:
        setName(tr("Other Systems"));
        break;
    case Qn::UsersNode:
        setName(tr("Users"));
        break;
    case Qn::WebPagesNode:
        setName(tr("Web Pages"));
        break;
    case Qn::UserResourcesNode:
        setName(tr("Cameras && Resources"));
        break;
    case Qn::LayoutsNode:
        setName(tr("Layouts"));
        break;
    case Qn::RecorderNode:
        m_state = Invalid;
        break;
    case Qn::SystemNode:
        m_state = Invalid;
        break;
    case Qn::AllCamerasAccessNode:
        setName(tr("All Cameras && Resources"));
        m_state = Invalid;
        break;
    case Qn::AllLayoutsAccessNode:
        setName(tr("All Shared Layouts"));
        m_state = Invalid;
        break;
    case Qn::SharedResourcesNode:
        setName(tr("Cameras && Resources"));
        m_state = Invalid;
        break;
    case Qn::RoleUsersNode:
        setName(tr("Users"));
        m_state = Invalid;
        break;
    default:
        break;
    }

    /* Invisible by default until has children. */
    m_bastard |= nodeRequiresChildren(nodeType);
}

/**
* Constructor for other virtual group nodes (recorders, other systems).
*/
QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, Qn::NodeType nodeType, const QString &name) :
    QnResourceTreeModelNode(model, nodeType)
{
    NX_ASSERT(nodeType == Qn::SystemNode || nodeType == Qn::RecorderNode);
    setName(name);
    if (m_displayName.isEmpty())
    {
        switch (nodeType)
        {
        case Qn::RecorderNode:
            break;
        case Qn::SystemNode:
            m_displayName = tr("<Unnamed system>");
            break;
        default:
            break;
        }
    }
    m_state = Invalid;
}

/**
 * Constructor for resource nodes.
 */
QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, const QnResourcePtr &resource, Qn::NodeType nodeType):
    QnResourceTreeModelNode(model, nodeType)
{
    NX_ASSERT(nodeType == Qn::ResourceNode
        || nodeType == Qn::EdgeNode
        || nodeType == Qn::SharedLayoutNode
        || nodeType == Qn::SharedResourceNode
    );
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
    NX_ASSERT(nodeType == Qn::LayoutItemNode
        || nodeType == Qn::VideoWallItemNode
        || nodeType == Qn::VideoWallMatrixNode
        || nodeType == Qn::RoleNode);

    m_state = Invalid;
    m_status = Qn::Offline;

    switch (nodeType)
    {
        case Qn::RoleNode:
        {
            auto role = qnUserRolesManager->userRole(m_uuid);
            setName(role.name);
            break;
        }
        default:
            break;
    }
}

QnResourceTreeModelNode::~QnResourceTreeModelNode()
{}

void QnResourceTreeModelNode::setResource(const QnResourcePtr& resource)
{
    if (m_resource == resource)
        return;

    NX_ASSERT(m_type == Qn::LayoutItemNode
        || m_type == Qn::ResourceNode
        || m_type == Qn::VideoWallItemNode
        || m_type == Qn::EdgeNode
        || m_type == Qn::SharedLayoutNode
        || m_type == Qn::SharedResourceNode
        || m_type == Qn::CurrentUserNode
    );

    if (m_resource)
        disconnect(m_resource, nullptr, this, nullptr);
    m_resource = resource;
    if (m_resource)
    {
        connect(resource, &QnResource::nameChanged, this, &QnResourceTreeModelNode::update);
        connect(resource, &QnResource::statusChanged, this, &QnResourceTreeModelNode::update);
        connect(resource, &QnResource::urlChanged, this, &QnResourceTreeModelNode::update);
        connect(resource, &QnResource::flagsChanged, this, &QnResourceTreeModelNode::update);

        if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        {
            connect(camera, &QnVirtualCameraResource::statusFlagsChanged, this,
                &QnResourceTreeModelNode::update);
        }
    }

    update();
}

void QnResourceTreeModelNode::update()
{
    /* Update stored fields. */
    switch (m_type)
    {
        case Qn::ResourceNode:
        case Qn::LayoutItemNode:
        case Qn::EdgeNode:
        case Qn::SharedLayoutNode:
        case Qn::SharedResourceNode:
        {
            if (!m_resource)
            {
                setName(QString());
                m_flags = 0;
                m_status = Qn::Online;
                m_searchString = QString();
            }
            else
            {
                m_name = m_resource->getName();
                m_flags = m_resource->flags();
                m_status = m_resource->getStatus();
                m_searchString = m_resource->toSearchString();
                m_displayName = QnResourceDisplayInfo(m_resource).toString(Qn::RI_NameOnly);
            }
            break;
        }
        case Qn::VideoWallItemNode:
        {
            m_status = Qn::Offline;

            QnVideoWallItemIndex index = qnResPool->getVideoWallItemByUuid(m_uuid);
            if (!index.isNull())
            {
                QnVideoWallItem item = index.item();

                if (item.runtimeStatus.online)
                {
                    if (item.runtimeStatus.controlledBy.isNull())
                        m_status = Qn::Online;
                    else if (item.runtimeStatus.controlledBy == qnCommon->moduleGUID())
                        m_status = Qn::Online;
                    else
                        m_status = Qn::Unauthorized;
                }

                setName(item.name);
            }
            else
            {
                setName(QString());
            }
            break;
        }
        case Qn::VideoWallMatrixNode:
        {
            for (const auto& videowall : qnResPool->getResources<QnVideoWallResource>())
            {
                if (!videowall->matrices()->hasItem(m_uuid))
                    continue;
                setName(videowall->matrices()->getItem(m_uuid).name);
                break;
            }
            break;
        }
        case Qn::CurrentSystemNode:
        {
            setName(qnGlobalSettings->systemName());
            break;
        }
        case Qn::CurrentUserNode:
        {
            auto user = context()->user();
            setName(user ? user->getName() : QString());
            break;
        }
        case Qn::RoleNode:
        {
            auto role = qnUserRolesManager->userRole(m_uuid);
            setName(role.name);
            break;
        }
        case Qn::SharedLayoutsNode:
        {
            if (m_parent && m_parent->type() == Qn::RoleNode)
                setName(tr("Shared Layouts"));
            else
                setName(tr("Layouts"));
            break;
        }
        default:
            break;
    }
    m_icon = calculateIcon();

    /* Update bastard state. */
    setBastard(calculateBastard());

    /* Notify views. */
    changeInternal();
}

void QnResourceTreeModelNode::updateRecursive()
{
    update();

    auto nodesToUpdate = m_model->children(toSharedPointer());
    for (auto child: nodesToUpdate)
        child->updateRecursive();
}

void QnResourceTreeModelNode::initialize()
{
    NX_ASSERT(!m_initialized);
    m_initialized = true;
}

void QnResourceTreeModelNode::deinitialize()
{
    NX_ASSERT(m_initialized);
    for (auto child: children())
        child->deinitialize();

    setParent(QnResourceTreeModelNodePtr());
    setResource(QnResourcePtr());
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

QnUuid QnResourceTreeModelNode::uuid() const
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
        /* Always hidden. */
        case Qn::BastardNode:
            return true;

        /* Always visible. */
        case Qn::RootNode:
        case Qn::LocalResourcesNode:
        case Qn::SeparatorNode:
        case Qn::LocalSeparatorNode:

        /* These will be hidden or displayed together with their parent. */
        case Qn::VideoWallItemNode:
        case Qn::VideoWallMatrixNode:
        case Qn::AllCamerasAccessNode:
        case Qn::AllLayoutsAccessNode:
        case Qn::SharedResourcesNode:
        case Qn::SharedLayoutsNode:
        case Qn::WebPagesNode:
        case Qn::RoleUsersNode:
        case Qn::SharedResourceNode:
        case Qn::RoleNode:
        case Qn::SharedLayoutNode:
        case Qn::RecorderNode:
        case Qn::SystemNode:
            return false;

        case Qn::CurrentSystemNode:
        case Qn::CurrentUserNode:
            return !isLoggedIn;

    /* Hide non-readable resources. */
    case Qn::LayoutItemNode:
    {
        /* Hide resource nodes without resource. */
        if (!m_resource)
            return true;

        return !accessController()->hasPermissions(m_resource, Qn::ReadPermission);
    }

    case Qn::OtherSystemsNode:
        return !isAdmin || !QnGlobalSettings::instance()->isAutoDiscoveryEnabled();

    case Qn::UsersNode:
    case Qn::ServersNode:
        return !isAdmin;

    case Qn::UserResourcesNode:
        return !isLoggedIn || isAdmin;

    case Qn::LayoutsNode:
        return !isLoggedIn;

    case Qn::EdgeNode:
        /* Hide resource nodes without resource. */
        if (!m_resource)
            return true;

        /* Only admins can see edge nodes. */
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
            if (layout->hasFlags(Qn::local) && !layout->isFile())
                return true;

            /* Hide "Preview Search" layouts */
            if (layout->data().contains(Qn::LayoutSearchStateRole)) //TODO: #GDM make it consistent with QnWorkbenchLayout::isSearchLayout
                return true;

            if (qnResPool->isAutoGeneratedLayout(layout))
                return true;

            return false;
        }

#ifndef DESKTOP_CAMERA_DEBUG
        /* Hide desktop camera resources from the tree. */
        if (m_flags.testFlag(Qn::desktop_camera))
            return true;
#endif
        /* Hide local server resource. */
        if (m_flags.testFlag(Qn::local_server))
            return true;

        /* Hide exported cameras inside of exported layouts. */
        if (m_flags.testFlag(Qn::exported_media))
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

        return false;

    default:
        NX_ASSERT(false, "Should never get here");
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

QList<QnResourceTreeModelNodePtr> QnResourceTreeModelNode::childrenRecursive() const
{
    QList<QnResourceTreeModelNodePtr> result(m_children);
    for (auto child: m_children)
        result << child->childrenRecursive();
    return result;
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
    if (Qn::isSeparatorNode(m_type)
        || m_type == Qn::AllCamerasAccessNode
        || m_type == Qn::AllLayoutsAccessNode)
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
    case Qn::SharedResourceNode:
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
            return m_displayName + (m_modified ? L'*' : QChar());
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
        return !m_searchString.isEmpty()
            ? m_searchString
            : m_displayName;
    case Qn::ResourceStatusRole:
        return QVariant::fromValue<int>(m_status);
    case Qn::NodeTypeRole:
        return qVariantFromValue(m_type);
    case Qn::UuidRole:
        return qVariantFromValue(m_uuid);

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

bool QnResourceTreeModelNode::isInitialized() const
{
    return m_initialized;
}

QnResourceTreeModel* QnResourceTreeModelNode::model() const
{
    return m_model;
}

void QnResourceTreeModelNode::handlePermissionsChanged(const QnResourcePtr& resource)
{
    if (resource == m_resource)
    {
        m_editable.checked = false;
        update();
    }
}

QIcon QnResourceTreeModelNode::calculateIcon() const
{
    switch (m_type)
    {
        case Qn::RootNode:
        case Qn::BastardNode:
        case Qn::SeparatorNode:
        case Qn::LocalSeparatorNode:
            return QIcon();

        case Qn::LocalResourcesNode:
            return qnResIconCache->icon(QnResourceIconCache::LocalResources);

        case Qn::CurrentSystemNode:
            return qnResIconCache->icon(QnResourceIconCache::CurrentSystem);

        case Qn::CurrentUserNode:
            return qnResIconCache->icon(QnResourceIconCache::User);

        case Qn::ServersNode:
            return qnResIconCache->icon(QnResourceIconCache::Servers);

        case Qn::OtherSystemsNode:
            return qnResIconCache->icon(QnResourceIconCache::OtherSystems);

        case Qn::UsersNode:
        case Qn::RoleNode:
        case Qn::RoleUsersNode:
            return qnResIconCache->icon(QnResourceIconCache::Users);

        case Qn::WebPagesNode:
            return qnResIconCache->icon(QnResourceIconCache::WebPages);

        case Qn::UserResourcesNode:
        case Qn::AllCamerasAccessNode:
        case Qn::SharedResourcesNode:
            return qnResIconCache->icon(QnResourceIconCache::Cameras);

        case Qn::LayoutsNode:
        case Qn::AllLayoutsAccessNode:
            return qnResIconCache->icon(QnResourceIconCache::Layouts);

        case Qn::RecorderNode:
            return qnResIconCache->icon(QnResourceIconCache::Recorder);

        case Qn::SystemNode:
            return qnResIconCache->icon(QnResourceIconCache::OtherSystem);

        case Qn::ResourceNode:
        case Qn::EdgeNode:
        case Qn::SharedLayoutNode:
            return m_resource ? qnResIconCache->icon(m_resource) : QIcon();

        case Qn::LayoutItemNode:
        case Qn::SharedResourceNode:
        {
            if (!m_resource)
                return QIcon();

            return m_resource->hasFlags(Qn::server)
                ? qnResIconCache->icon(QnResourceIconCache::HealthMonitor)
                : qnResIconCache->icon(m_resource);
        }

        case Qn::SharedLayoutsNode:
        {
            if (m_parent && m_parent->type() == Qn::RoleNode)
                return qnResIconCache->icon(QnResourceIconCache::SharedLayout);

            return qnResIconCache->icon(QnResourceIconCache::Layouts);
        }

        case Qn::VideoWallMatrixNode:
            return qnResIconCache->icon(QnResourceIconCache::VideoWallMatrix);

        case Qn::VideoWallItemNode:
        {
            QnVideoWallItemIndex index = qnResPool->getVideoWallItemByUuid(m_uuid);
            if (!index.isNull())
            {
                QnVideoWallItem item = index.item();
                if (item.runtimeStatus.online)
                {
                    if (item.runtimeStatus.controlledBy.isNull())
                        return qnResIconCache->icon(QnResourceIconCache::VideoWallItem);

                    if (item.runtimeStatus.controlledBy == qnCommon->moduleGUID())
                    {
                        return qnResIconCache->icon(QnResourceIconCache::VideoWallItem
                            | QnResourceIconCache::Control);
                    }

                    return qnResIconCache->icon(QnResourceIconCache::VideoWallItem
                        | QnResourceIconCache::Locked);
                }
            }

            return qnResIconCache->icon(QnResourceIconCache::VideoWallItem
                | QnResourceIconCache::Offline);
        }

        default:
            break;
    }

    return QIcon();
}

void QnResourceTreeModelNode::removeChildInternal(const QnResourceTreeModelNodePtr& child)
{
    NX_ASSERT(child->parent() == this);
    NX_ASSERT(m_children.contains(child));

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

void QnResourceTreeModelNode::setName(const QString& name)
{
    m_displayName = m_name = name;
}


QDebug operator<<(QDebug dbg, QnResourceTreeModelNode* node)
{
    if (!node)
        return dbg.space() << "INVALID";

    auto p = node->parent();
    if (p)
        dbg.nospace() << "+";
    while (p)
    {
        dbg.nospace() << "-";
        p = p->parent();
    }

    /* Print name. */
    dbg.nospace() << node->data(Qt::EditRole, Qn::NameColumn).toString() << " (" << node->type() << ")";
    bool bastard = node->parent()
        && !node->parent()->children().contains(node->toSharedPointer());
    if (bastard)
        dbg.nospace() << " (bastard)";

    dbg.nospace() << "\n";

    for (auto c: node->children())
        dbg.nospace() << c;

    return dbg.nospace();
}

QDebug operator<<(QDebug dbg, const QnResourceTreeModelNodePtr& node)
{
    return dbg << node.data();
}
