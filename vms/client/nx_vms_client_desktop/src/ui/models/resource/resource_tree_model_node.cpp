#include "resource_tree_model_node.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_management/layout_tour_manager.h>

#include <core/resource_access/resource_access_manager.h>

#include <core/resource/resource_display_info.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>

#include <network/system_description.h>

#include <api/global_settings.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ini.h>

#include <ui/help/help_topics.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/tree/resource_tree_model_node_manager.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>


using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

/* Set of node types, that are require children to be visible. */
bool nodeRequiresChildren(ResourceTreeNodeType nodeType)
{
    switch (nodeType)
    {
        case ResourceTreeNodeType::otherSystems:
        case ResourceTreeNodeType::servers:
        case ResourceTreeNodeType::filteredServers:
        case ResourceTreeNodeType::filteredCameras:
        case ResourceTreeNodeType::filteredLayouts:
        case ResourceTreeNodeType::filteredUsers:
        case ResourceTreeNodeType::filteredVideowalls:
        case ResourceTreeNodeType::userResources:
        case ResourceTreeNodeType::recorder:
        case ResourceTreeNodeType::localSystem:
        case ResourceTreeNodeType::roleUsers:
        case ResourceTreeNodeType::layouts:
        case ResourceTreeNodeType::sharedResources:
        case ResourceTreeNodeType::sharedLayouts:
        case ResourceTreeNodeType::layoutTours:
            return true;
        default:
            return false;
    }
}

} // namespace

QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, NodeType nodeType, const QnUuid& uuid) :
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
    m_checkState(Qt::Unchecked),
    m_uncheckedChildren(0),
    m_checkedChildren(0)
{
    NX_ASSERT(model != NULL);
    m_editable.checked = false;
    m_icon = calculateIcon();
}

QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, NodeType nodeType):
    QnResourceTreeModelNode(model, nodeType, QnUuid())
{
    switch(nodeType)
    {
    case NodeType::root:
        setNameInternal(lit("Root"));   /* This node is not visible directly. */
        break;
    case NodeType::bastard:
        setNameInternal(lit("Invalid Resources"));
        m_bastard = true; /* This node is always hidden. */
        m_state = Invalid;
        break;
    case NodeType::localResources:
        setNameInternal(tr("Local Files"));
        break;
    case NodeType::currentSystem:
        break;
    case NodeType::currentUser:
        m_flags = Qn::user;
        break;
    case NodeType::separator:
    case NodeType::localSeparator:
        m_displayName = QString();
        m_name = lit("-");
        break;
    case NodeType::filteredServers:
    case NodeType::servers:
        setNameInternal(tr("Servers"));
        break;
    case NodeType::otherSystems:
        setNameInternal(tr("Other Systems"));
        m_state = Invalid;
        break;
    case NodeType::filteredUsers:
    case NodeType::users:
        setNameInternal(tr("Users"));
        break;
    case NodeType::webPages:
        setNameInternal(tr("Web Pages"));
        break;
    case NodeType::filteredCameras:
        setNameInternal(tr("Cameras & Devices"));
        break;
    case NodeType::userResources:
        setNameInternal(tr("Cameras & Resources"));
        break;
    case NodeType::filteredLayouts:
    case NodeType::layouts:
        setNameInternal(tr("Layouts"));
        break;
    case NodeType::filteredVideowalls:
        setNameInternal(tr("Videowalls"));
        break;
    case NodeType::layoutTours:
        setNameInternal(tr("Showreels"));
        break;
    case NodeType::analyticsEngines:
    case NodeType::filteredAnalyticsEngines:
        setNameInternal(tr("Analytics Engines"));
        m_state = Invalid;
        break;
    case NodeType::recorder:
        m_state = Invalid;
        break;
    case NodeType::localSystem:
        m_state = Invalid;
        break;
    case NodeType::allCamerasAccess:
        setNameInternal(tr("All Cameras & Resources"));
        m_state = Invalid;
        break;
    case NodeType::allLayoutsAccess:
        setNameInternal(tr("All Shared Layouts"));
        m_state = Invalid;
        break;
    case NodeType::sharedResources:
        setNameInternal(tr("Cameras & Resources"));
        m_state = Invalid;
        break;
    case NodeType::roleUsers:
        setNameInternal(tr("Users"));
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
QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, NodeType nodeType, const QString &name) :
    QnResourceTreeModelNode(model, nodeType)
{
    NX_ASSERT(
        nodeType == NodeType::localSystem
        || nodeType == NodeType::recorder
        || nodeType == NodeType::cloudSystem
    );
    setName(name);
    m_state = Invalid;
}

/**
 * Constructor for resource nodes.
 */
QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, const QnResourcePtr &resource, NodeType nodeType):
    QnResourceTreeModelNode(model, nodeType)
{
    NX_ASSERT(nodeType == NodeType::resource
        || nodeType == NodeType::edge
        || nodeType == NodeType::sharedLayout
        || nodeType == NodeType::sharedResource
    );
    m_state = Invalid;
    m_status = Qn::Offline;
    setResource(resource);
}

/**
 * Constructor for item nodes.
 */
QnResourceTreeModelNode::QnResourceTreeModelNode(QnResourceTreeModel* model, const QnUuid &uuid, NodeType nodeType):
    QnResourceTreeModelNode(model, nodeType, uuid)
{
    NX_ASSERT(nodeType == NodeType::layoutItem
        || nodeType == NodeType::videoWallItem
        || nodeType == NodeType::videoWallMatrix
        || nodeType == NodeType::role
        || nodeType == NodeType::layoutTour
    );

    m_state = Invalid;
    m_status = Qn::Offline;

    switch (nodeType)
    {
        case NodeType::role:
        {
            auto role = userRolesManager()->userRole(m_uuid);
            setNameInternal(role.name);
            break;
        }
        case NodeType::layoutTour:
        {
            auto tour = layoutTourManager()->tour(m_uuid);
            setNameInternal(tour.name);
            break;
        }
        default:
            break;
    }
}

QnResourceTreeModelNode::~QnResourceTreeModelNode()
{
    NX_ASSERT(m_resource.isNull());
}

void QnResourceTreeModelNode::setResource(const QnResourcePtr& resource)
{
    if (m_resource == resource)
        return;

    NX_ASSERT(m_type == NodeType::layoutItem
        || m_type == NodeType::resource
        || m_type == NodeType::videoWallItem
        || m_type == NodeType::edge
        || m_type == NodeType::sharedLayout
        || m_type == NodeType::sharedResource
        || m_type == NodeType::currentUser);

    if (m_initialized)
    {
        auto nodePtr = toSharedPointer();
        NX_ASSERT(!nodePtr.isNull());
        if (m_resource)
            manager()->removeResourceNode(nodePtr);
        m_resource = resource;
        if (resource)
            manager()->addResourceNode(nodePtr);
    }
    else
    {
        /* Called from a constructor: */
        m_resource = resource;
    }

    update();
}

QnResourceTreeModelNodeManager* QnResourceTreeModelNode::manager() const
{
    return model()->nodeManager();
}

void QnResourceTreeModelNode::update()
{
    /* Update stored fields. */
    switch (m_type)
    {
        case NodeType::resource:
        case NodeType::layoutItem:
        case NodeType::edge:
        case NodeType::sharedLayout:
        case NodeType::sharedResource:
        {
            if (!m_resource)
            {
                setNameInternal(QString());
                m_flags = 0;
                m_status = Qn::Online;
                m_cameraExtraStatus = {};
            }
            else
            {
                m_name = m_resource->getName();
                m_flags = m_resource->flags();
                m_status = m_resource->getStatus();
                m_displayName = QnResourceDisplayInfo(m_resource).toString(Qn::RI_NameOnly);
                m_cameraExtraStatus = calculateCameraExtraStatus();
            }
            break;
        }
        case NodeType::videoWallItem:
        {
            m_status = Qn::Offline;

            QnVideoWallItemIndex index = resourcePool()->getVideoWallItemByUuid(m_uuid);
            if (!index.isNull())
            {
                QnVideoWallItem item = index.item();

                if (item.runtimeStatus.online)
                {
                    if (item.runtimeStatus.controlledBy.isNull())
                        m_status = Qn::Online;
                    else if (item.runtimeStatus.controlledBy == commonModule()->moduleGUID())
                        m_status = Qn::Online;
                    else
                        m_status = Qn::Unauthorized;
                }

                setNameInternal(item.name);
            }
            else
            {
                setNameInternal(QString());
            }
            break;
        }
        case NodeType::videoWallMatrix:
        {
            for (const auto& videowall : resourcePool()->getResources<QnVideoWallResource>())
            {
                if (!videowall->matrices()->hasItem(m_uuid))
                    continue;
                setNameInternal(videowall->matrices()->getItem(m_uuid).name);
                break;
            }
            break;
        }
        case NodeType::layoutTour:
        {
            auto tour = layoutTourManager()->tour(m_uuid);
            setNameInternal(tour.name.isEmpty() ? tr("Showreel") : tour.name);
            break;
        }
        case NodeType::currentSystem:
        {
            setNameInternal(qnGlobalSettings->systemName());
            break;
        }
        case NodeType::currentUser:
        {
            auto user = context()->user();
            setNameInternal(user ? user->getName() : QString());
            break;
        }
        case NodeType::role:
        {
            auto role = userRolesManager()->userRole(m_uuid);
            setNameInternal(role.name);
            break;
        }
        case NodeType::sharedLayouts:
        {
            if (m_parent && m_parent->type() == NodeType::role)
                setNameInternal(tr("Shared Layouts"));
            else
                setNameInternal(tr("Layouts"));
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

    /* If setResource was called from the constructor: */
    if (m_resource)
        manager()->addResourceNode(toSharedPointer());
}

void QnResourceTreeModelNode::deinitialize()
{
    if (!m_initialized)
        return;

    for (auto child: children())
        child->deinitialize();

    setParent(QnResourceTreeModelNodePtr());
    setResource(QnResourcePtr());

    m_state = Invalid;
    m_initialized = false;
}

ResourceTreeNodeType QnResourceTreeModelNode::type() const
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
    bool isAdmin = accessController()->hasGlobalPermission(GlobalPermission::admin);

    switch (m_type)
    {
        /* Always hidden. */
        case NodeType::bastard:
            return true;

        /* Always visible. */
        case NodeType::root:
        case NodeType::localResources:
        case NodeType::separator:
        case NodeType::localSeparator:
        case NodeType::otherSystems:

        case NodeType::filteredServers:
        case NodeType::filteredCameras:
        case NodeType::filteredLayouts:
        case NodeType::filteredUsers:
        case NodeType::filteredVideowalls:


        /* These will be hidden or displayed together with their parent. */
        case NodeType::videoWallItem:
        case NodeType::videoWallMatrix:
        case NodeType::allCamerasAccess:
        case NodeType::allLayoutsAccess:
        case NodeType::sharedResources:
        case NodeType::sharedLayouts:
        case NodeType::webPages:
        case NodeType::roleUsers:
        case NodeType::sharedResource:
        case NodeType::role:
        case NodeType::layoutTour:
        case NodeType::sharedLayout:
        case NodeType::recorder:
        case NodeType::localSystem:
        case NodeType::cloudSystem:
            return false;

        case NodeType::currentSystem:
        case NodeType::currentUser:
            return !isLoggedIn;

    /* Hide non-readable resources. */
    case NodeType::layoutItem:
    {
        /* Hide resource nodes without resource. */
        if (!m_resource)
            return true;

        return !accessController()->hasPermissions(m_resource, Qn::ViewContentPermission);
    }

    case NodeType::users:
    case NodeType::servers:
        return !isAdmin;

    case NodeType::userResources:
        return !isLoggedIn || isAdmin;

    case NodeType::layouts:
    case NodeType::layoutTours:
        return !isLoggedIn;

    case NodeType::edge:
        /* Hide resource nodes without resource. */
        if (!m_resource)
            return true;

        /* Only admins can see edge nodes. */
        return !isAdmin;

    case NodeType::analyticsEngines:
    case NodeType::filteredAnalyticsEngines:
        return !ini().displayAnalyticsEnginesInResourceTree;

    case NodeType::resource:
        /* Hide resource nodes without resource. */
        if (!m_resource)
            return true;

        /* Other systems visibility governed by its parent node. */
        if (m_resource->hasFlags(Qn::fake))
            return false;

        /* Hide non-readable resources. */
        if (!accessController()->hasPermissions(m_resource, Qn::ReadPermission))
            return true;

        if (QnLayoutResourcePtr layout = m_resource.dynamicCast<QnLayoutResource>())
        {
            /* Hide local layouts that are not file-based. */
            if (layout->hasFlags(Qn::local) && !layout->isFile())
                return true;

            if (layout->isServiceLayout())
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

        // Hide exported camera resources inside of exported layouts. Layout items are displayed instead.
        if (m_flags.testFlag(Qn::exported_media)
            && m_parent
            && m_parent->m_flags.testFlag(Qn::exported_layout))
        {
            return true;
        }

        /* Hide edge servers, camera will be displayed instead. */
        if (QnMediaServerResource::isHiddenServer(m_resource) &&
            !resourcePool()->getResourcesByParentId(m_resource->getId()).filtered<QnVirtualCameraResource>().isEmpty())
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

QnResourceTreeModelNodePtr QnResourceTreeModelNode::child(int index) const
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

            if (m_type == NodeType::videoWallItem)
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
    if (isSeparatorNode(m_type)
        || m_type == NodeType::allCamerasAccess
        || m_type == NodeType::allLayoutsAccess)
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
        case NodeType::sharedResource:
        case NodeType::sharedLayout:
        case NodeType::resource:
        case NodeType::edge:
            m_editable.value = menu()->canTrigger(action::RenameResourceAction, m_resource);
            break;
        case NodeType::videoWallItem:
        case NodeType::videoWallMatrix:
            m_editable.value = accessController()->hasGlobalPermission(GlobalPermission::controlVideowall);
            break;
        case NodeType::layoutTour:
            m_editable.value = menu()->canTrigger(action::RenameLayoutTourAction);
            break;
        case NodeType::recorder:
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
    case NodeType::resource:
    case NodeType::edge:
    case NodeType::layoutItem:
    case NodeType::sharedLayout:
    case NodeType::sharedResource:
    {
        // Any of flags is sufficient.
        if (m_flags & (Qn::media | Qn::layout | Qn::server | Qn::videowall))
            result |= Qt::ItemIsDragEnabled;

        // Web page is a combination of flags.
        if (m_flags.testFlag(Qn::web_page))
            result |= Qt::ItemIsDragEnabled;
        break;
    }
    case NodeType::videoWallItem: // TODO: #GDM #VW drag of empty item on scene should create new layout
    case NodeType::recorder:
    case NodeType::layoutTour:
        result |= Qt::ItemIsDragEnabled;
        break;
    default:
        break;
    }
    return result;
}

QVariant QnResourceTreeModelNode::data(int role, int column) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
            if (column == Qn::NameColumn)
            {
                return m_modified
                    ? m_displayName + L'*'
                    : m_displayName;
            }
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
            if (m_type == NodeType::layoutItem
                || m_type == NodeType::videoWallItem
                || m_type == NodeType::videoWallMatrix)
            {
                return QVariant::fromValue<QnUuid>(m_uuid);
            }
            break;

        case Qn::ResourceStatusRole:
            return QVariant::fromValue<int>(m_status);

        case Qn::CameraExtraStatusRole:
            return qVariantFromValue(m_cameraExtraStatus);

        case Qn::NodeTypeRole:
            return qVariantFromValue(m_type);

        case Qn::UuidRole:
            return qVariantFromValue(m_uuid);

        case Qn::HelpTopicIdRole:
            return helpTopicId();

        default:
            break;
    }

    return QVariant();
}

int QnResourceTreeModelNode::helpTopicId() const
{
    switch (m_type)
    {
        case NodeType::users:
            return Qn::MainWindow_Tree_Users_Help;

        case NodeType::localResources:
            return Qn::MainWindow_Tree_Local_Help;

        case NodeType::recorder:
            return Qn::MainWindow_Tree_Recorder_Help;

        case NodeType::videoWallItem:
            return Qn::Videowall_Display_Help;

        case NodeType::videoWallMatrix:
            return Qn::Videowall_Matrix_Help;

        default:
            break;
    }

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

    if (m_flags.testFlag(Qn::web_page))
        return Qn::MainWindow_Tree_WebPage_Help;

    return -1;
}

bool QnResourceTreeModelNode::setData(const QVariant& value, int role, int column)
{
    if (column == Qn::CheckColumn && role == Qt::CheckStateRole)
    {
        auto checkState = static_cast<Qt::CheckState>(value.toInt());

        if (checkState == Qt::PartiallyChecked)
            return false; //< setting PartiallyChecked from outside is prohibited

        if (!changeCheckStateRecursivelyUp(checkState))
            return false;

        propagateCheckStateRecursivelyDown();
        return true;
    }

    if (role != Qt::EditRole)
        return false;

    if (value.toString().isEmpty())
        return false;

    action::Parameters parameters;
    bool isVideoWallEntity = false;
    if (m_type == NodeType::videoWallItem)
    {
        // TODO: #GDM #3.1 get rid of all this logic, just pass uuid
        QnVideoWallItemIndex index = resourcePool()->getVideoWallItemByUuid(m_uuid);
        if (index.isNull())
            return false;
        parameters = (QnVideoWallItemIndexList() << index);
        isVideoWallEntity = true;
    }
    else if (m_type == NodeType::videoWallMatrix)
    {
        QnVideoWallMatrixIndex index = resourcePool()->getVideoWallMatrixByUuid(m_uuid);
        if (index.isNull())
            return false;
        parameters = (QnVideoWallMatrixIndexList() << index);
        isVideoWallEntity = true;
    }
    else if (m_type == NodeType::recorder)
    {
        //sending first camera to get groupId and check WriteName permission
        if (children().isEmpty())
            return false;

        auto child = this->child(0);
        if (!child->resource())
            return false;
        parameters = child->resource();
    }
    else
    {
        parameters = m_resource;
    }
    parameters.setArgument(Qn::ResourceNameRole, value.toString());
    parameters.setArgument(Qn::NodeTypeRole, m_type);
    parameters.setArgument(Qn::UuidRole, m_uuid);

    if (m_type == NodeType::layoutTour)
        menu()->trigger(action::RenameLayoutTourAction, parameters);
    else if (isVideoWallEntity)
        menu()->trigger(action::RenameVideowallEntityAction, parameters);
    else
        menu()->trigger(action::RenameResourceAction, parameters);
    return true;
}

bool QnResourceTreeModelNode::changeCheckStateRecursivelyUp(Qt::CheckState newState)
{
    if (m_checkState == newState)
        return false;

    auto oldState = m_checkState;

    m_checkState = newState;

    if (m_parent)
        m_parent->childCheckStateChanged(oldState, newState);

    changeInternal();
    return true;
}

void QnResourceTreeModelNode::childCheckStateChanged(Qt::CheckState oldState, Qt::CheckState newState, bool forceUpdate)
{
    if (oldState == newState && !forceUpdate)
        return;

    if (oldState != newState)
    {
        switch (oldState)
        {
            case Qt::Unchecked:
                --m_uncheckedChildren;
                NX_ASSERT(m_uncheckedChildren >= 0);
                break;

            case Qt::Checked:
                --m_checkedChildren;
                NX_ASSERT(m_checkedChildren >= 0);
                break;

            default:
                break;
        }

        switch (newState)
        {
            case Qt::Unchecked:
                ++m_uncheckedChildren;
                break;

            case Qt::Checked:
                ++m_checkedChildren;
                break;

            default:
                break;
        }

        NX_ASSERT(m_checkedChildren + m_uncheckedChildren <= m_children.size());
    }

    if (m_uncheckedChildren == m_children.size())
        changeCheckStateRecursivelyUp(Qt::Unchecked);
    else if (m_checkedChildren == m_children.size())
        changeCheckStateRecursivelyUp(Qt::Checked);
    else
        changeCheckStateRecursivelyUp(Qt::PartiallyChecked);
}

void QnResourceTreeModelNode::propagateCheckStateRecursivelyDown()
{
    if (m_checkState == Qt::PartiallyChecked)
    {
        NX_ASSERT(false); //< should not happen
        return;
    }

    /* Update counters at each child state change to keep them consistent
     * with current state every time when dataChanged signal is emitted: */
    auto& increasing = m_checkState == Qt::Checked
        ? m_checkedChildren
        : m_uncheckedChildren;

    auto& decreasing = m_checkState == Qt::Checked
        ? m_uncheckedChildren
        : m_checkedChildren;

    for (const auto& child: m_children)
    {
        if (child->m_checkState == m_checkState)
            continue;

        if (child->m_checkState != Qt::PartiallyChecked)
        {
            ++increasing;
            --decreasing;
        }

        child->m_checkState = m_checkState;
        child->propagateCheckStateRecursivelyDown();
        child->changeInternal();
    }

    NX_ASSERT(decreasing == 0 && increasing == m_children.size());
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

void QnResourceTreeModelNode::setName(const QString& name)
{
    bool changed = m_name != name;

    setNameInternal(name);

    if (m_displayName.isEmpty())
    {
        switch (m_type)
        {
            case NodeType::localSystem:
            case NodeType::cloudSystem:
                m_displayName = QnSystemDescription::extractSystemName(m_displayName);
                changed = true;
                break;
            default:
                break;
        }
    }

    if (changed)
        changeInternal();
}

void QnResourceTreeModelNode::setIcon(const QIcon& icon)
{
    m_icon = icon;
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

void QnResourceTreeModelNode::handlePermissionsChanged()
{
    m_editable.checked = false;
    update();
}

QIcon QnResourceTreeModelNode::calculateIcon() const
{
    switch (m_type)
    {
        case NodeType::root:
        case NodeType::bastard:
        case NodeType::separator:
        case NodeType::localSeparator:
            return QIcon();

        case NodeType::localResources:
            return qnResIconCache->icon(QnResourceIconCache::LocalResources);

        case NodeType::currentSystem:
            return qnResIconCache->icon(QnResourceIconCache::CurrentSystem);

        case NodeType::currentUser:
            return qnResIconCache->icon(QnResourceIconCache::User);

        case NodeType::filteredServers:
        case NodeType::servers:
            return qnResIconCache->icon(QnResourceIconCache::Servers);

        case NodeType::users:
        case NodeType::role:
        case NodeType::roleUsers:
        case NodeType::filteredUsers:
            return qnResIconCache->icon(QnResourceIconCache::Users);

        case NodeType::webPages:
            return qnResIconCache->icon(QnResourceIconCache::WebPages);

        case NodeType::analyticsEngines:
            return qnResIconCache->icon(QnResourceIconCache::AnalyticsEngines);

        case NodeType::filteredVideowalls:
            return qnResIconCache->icon(QnResourceIconCache::VideoWall); //< Fix me: change to videowallS icon

        case NodeType::filteredCameras:
        case NodeType::userResources:
        case NodeType::allCamerasAccess:
        case NodeType::sharedResources:
            return qnResIconCache->icon(QnResourceIconCache::Cameras);

        case NodeType::filteredLayouts:
        case NodeType::layouts: //< Overridden in QnResourceTreeModelLayoutNode.
            return qnResIconCache->icon(QnResourceIconCache::Layouts);

        case NodeType::layoutTour:
            return qnResIconCache->icon(QnResourceIconCache::LayoutTour);

        case NodeType::layoutTours:
            return qnResIconCache->icon(QnResourceIconCache::LayoutTours);

        case NodeType::allLayoutsAccess:
            return qnResIconCache->icon(QnResourceIconCache::SharedLayouts);

        case NodeType::localSystem:
            return qnResIconCache->icon(QnResourceIconCache::OtherSystem);

        case NodeType::resource:
        case NodeType::edge:
        case NodeType::sharedLayout:
            return m_resource ? qnResIconCache->icon(m_resource) : QIcon();

        case NodeType::layoutItem:
        case NodeType::sharedResource:
        {
            if (!m_resource)
                return QIcon();

            if (!m_resource->hasFlags(Qn::server))
                return qnResIconCache->icon(m_resource);

            return m_resource->getStatus() == Qn::Offline
                ? qnResIconCache->icon(QnResourceIconCache::HealthMonitor | QnResourceIconCache::Offline)
                : qnResIconCache->icon(QnResourceIconCache::HealthMonitor);
        }

        case NodeType::sharedLayouts:
        {
            if (m_parent && m_parent->type() == NodeType::role)
                return qnResIconCache->icon(QnResourceIconCache::SharedLayouts);

            return qnResIconCache->icon(QnResourceIconCache::Layouts);
        }

        case NodeType::videoWallMatrix:
            return qnResIconCache->icon(QnResourceIconCache::VideoWallMatrix);

        case NodeType::videoWallItem:
        {
            QnVideoWallItemIndex index = resourcePool()->getVideoWallItemByUuid(m_uuid);
            if (!index.isNull())
            {
                QnVideoWallItem item = index.item();
                if (item.runtimeStatus.online)
                {
                    if (item.runtimeStatus.controlledBy.isNull())
                        return qnResIconCache->icon(QnResourceIconCache::VideoWallItem);

                    if (item.runtimeStatus.controlledBy == commonModule()->moduleGUID())
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

CameraExtraStatus QnResourceTreeModelNode::calculateCameraExtraStatus() const
{
    if (const auto camera = m_resource.dynamicCast<QnVirtualCameraResource>())
        return getCameraExtraStatus(camera);
    return CameraExtraStatus();
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

    /* To subtract from checked/unchecked children counter & update parents recursively: */
    childCheckStateChanged(child->m_checkState, Qt::PartiallyChecked, true);

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

    /* To add to checked/unchecked children counter & update parents recursively: */
    childCheckStateChanged(Qt::PartiallyChecked, child->m_checkState, true);

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

void QnResourceTreeModelNode::updateIcon()
{
    m_icon = calculateIcon();
    changeInternal();
}

void QnResourceTreeModelNode::setNameInternal(const QString& name)
{
    m_displayName = m_name = name;
}

void QnResourceTreeModelNode::updateResourceStatus()
{
    NX_ASSERT(m_resource);
    if (!m_resource)
        return;

    m_status = m_resource->getStatus();
    m_cameraExtraStatus = calculateCameraExtraStatus();
    m_icon = calculateIcon();
    changeInternal();

    if (m_parent && m_parent->type() == NodeType::recorder)
        m_parent->updateCameraExtraStatus();
}

void QnResourceTreeModelNode::updateCameraExtraStatus()
{
    m_cameraExtraStatus = calculateCameraExtraStatus();
    changeInternal();

    if (m_parent && m_parent->type() == NodeType::recorder)
        m_parent->updateCameraExtraStatus();
}

bool QnResourceTreeModelNode::isPrimary() const
{
    return m_prev.isNull();
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
    dbg.nospace()
        << node->data(Qt::EditRole, Qn::NameColumn).toString()
        << " ("
        << (int)node->type()
        << ")";

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
