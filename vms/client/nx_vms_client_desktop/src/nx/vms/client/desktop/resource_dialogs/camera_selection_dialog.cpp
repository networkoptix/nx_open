#include "camera_selection_dialog.h"

#include "ui_camera_selection_dialog.h"
#include "details/filtered_resource_selection_widget.h"

#include <common/common_module.h>
#include <ui/workbench/workbench_context.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/global_permissions_manager.h>
#include <nx_ec/access_helpers.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_selection_node_view.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>
#include <client/client_settings.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace node_view;
using namespace details;


struct Data
{
    QHash<QnUuid, QnVirtualCameraResourcePtr> allCameras;
    QnUuidSet serverIds;

    QHash<QnUuid, QnUuidSet> singleCamerasByServerId;
    QHash<QnUuid, QSet<QString>> cameraGroupsByServerId;
    QHash<QString, QnUuidSet> singleCamerasByGroupId;

    QnUuidSet invalidCameras;
    QnUuidSet selectedInvalidCameras;
};

Data createCamerasData(
    const QnUserResourcePtr& currentUser,
    const QnCommonModule* commonModule,
    const QnUuidSet& selectedCameras,
    const CameraSelectionDialog::ValidResourceCheck& validCheck)
{
    Data result;

    using namespace ec2::access_helpers;
    const auto pool = commonModule->resourcePool();
    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto accessibleCameras = getAccessibleResources(
        currentUser, pool->getAllCameras(QnResourcePtr(), true), accessProvider);

    for (const auto camera: accessibleCameras)
    {
        const auto parentServerId = camera->getParentServer()->getId();
        const auto cameraId = camera->getId();

        if (!camera->getGroupId().isNull())
        {
            const auto cameraGroupId = camera->getGroupId();
            result.cameraGroupsByServerId[parentServerId].insert(cameraGroupId);
            result.singleCamerasByGroupId[cameraGroupId].insert(cameraId);
        }
        else
        {
            result.singleCamerasByServerId[parentServerId].insert(cameraId);
        }

        result.serverIds.insert(parentServerId);
        result.allCameras.insert(cameraId, camera);

        if (!validCheck || validCheck(camera))
            continue;

        result.invalidCameras.insert(cameraId);
        if (selectedCameras.contains(cameraId))
            result.selectedInvalidCameras.insert(cameraId);
    }

    return result;
}

NodePtr createServerNode(
    const QnUuid& serverId,
    QnResourcePool* pool,
    const NodeList& children)
{
    auto serverResource = pool->getResourceById(serverId);
    const bool showExtraInfo = qnSettings->extraInfoInTree() == Qn::RI_FullInfo;
    const auto extraInfoText =
        showExtraInfo ? QnResourceDisplayInfo(serverResource).extraInfo() : QString();

    const auto extraText = lit("%1 - %2").arg(extraInfoText,
        CameraSelectionDialog::tr("%n cameras", nullptr, children.size())).trimmed();

    return createResourceNode(serverResource, extraText, true);
}

NodePtr createCameraNodes(
    const Data& data,
    bool adminMode,
    bool showInvalidCameras,
    QnResourcePool* pool)
{
    const NodePtr root = ViewNode::create();
    const bool showExtraInfo = qnSettings->extraInfoInTree() == Qn::RI_FullInfo;

    const auto createCameraNode =
        [pool, showExtraInfo](const QnUuid& cameraId, bool isInvalidCamera) -> NodePtr
        {
            auto resource = pool->getResourceById(cameraId);
            QString extraText =
                showExtraInfo ? QnResourceDisplayInfo(resource).extraInfo() : QString();
            const auto cameraNode = createResourceNode(resource, extraText, true);
            if (isInvalidCamera)
                setNodeValidState(cameraNode, false);
            return cameraNode;
        };

    for (auto serverId: data.serverIds)
    {
        NodeList children;

        if (data.cameraGroupsByServerId.contains(serverId))
        {
            QSet<QString> groupIds = data.cameraGroupsByServerId.value(serverId);
            for (const QString& groupId: groupIds)
            {
                NodeList groupChildren;
                for (const auto cameraId: data.singleCamerasByGroupId.value(groupId))
                {
                    auto isInvalidCamera = data.invalidCameras.contains(cameraId);
                    if (showInvalidCameras || !isInvalidCamera)
                        groupChildren.append(createCameraNode(cameraId, isInvalidCamera));
                }

                auto groupResource =
                    data.allCameras.value(pool->getResourcesBySharedId(groupId).first()->getId());
                QString groupExtraText =
                    showExtraInfo ? QnResourceDisplayInfo(groupResource).extraInfo() : QString();
                const auto groupNode = createGroupNode(groupResource, groupExtraText, true);
                groupNode->addChildren(groupChildren);

                children.append(groupNode);
            }
        }

        auto singleCameraIds = data.singleCamerasByServerId.value(serverId);
        for (const auto cameraId: singleCameraIds)
        {
            auto isInvalidCamera = data.invalidCameras.contains(cameraId);
            if (showInvalidCameras || !isInvalidCamera)
                children.append(createCameraNode(cameraId, isInvalidCamera));
        }

        if (children.isEmpty())
            continue;

        const NodePtr targetNode = adminMode ? createServerNode(serverId, pool, children) : root;
        targetNode->addChildren(children);
        if (adminMode)
            root->addChild(targetNode);
    }

    return root;
}

} // namespace

namespace nx::vms::client::desktop {

struct CameraSelectionDialog::Private: public QObject
{
    Private(
        CameraSelectionDialog* owner,
        const ValidResourceCheck& validResourceCheck,
        const GetText& getText,
        const QnUuidSet& selectedCameras);

    void handleSelectionChanged(const QnUuid& resourceId, Qt::CheckState checkedState);
    void reloadViewData();

    /**
     * Allows to show invalid cameras depending on specified value.
     * @return True if nodes were updated, otherwise false.
     */
    bool setShowInvalidCameras(bool value);
    void setLockCurrentMode(bool force);

    const CameraSelectionDialog* q;
    const GetText getText;
    const QnUserResourcePtr currentUser;
    const bool isAdminUser;
    QnUuidSet selectedCameras;
    Data data;
    bool showInvalidCameras = false;
};

CameraSelectionDialog::Private::Private(
    CameraSelectionDialog* owner,
    const ValidResourceCheck& validResourceCheck,
    const GetText& getText,
    const QnUuidSet& selectedCameras)
    :
    q(owner),
    getText(getText),
    currentUser(q->context()->user()),
    isAdminUser(q->globalPermissionsManager()->hasGlobalPermission(
        currentUser, GlobalPermission::adminPermissions)),
    selectedCameras(selectedCameras),
    data(createCamerasData(currentUser, q->commonModule(), selectedCameras, validResourceCheck))
{
}

void CameraSelectionDialog::Private::handleSelectionChanged(
    const QnUuid& resourceId,
    Qt::CheckState checkedState)
{
    if (!data.allCameras.contains(resourceId))
        return;

    switch (checkedState)
    {
        case Qt::Checked:
            selectedCameras.insert(resourceId);
            if (data.invalidCameras.contains(resourceId))
            {
                data.selectedInvalidCameras.insert(resourceId);
                setLockCurrentMode(true);
            }
            if (!data.selectedInvalidCameras.isEmpty() && getText)
            {
                QnResourceList selectedResources;
                for (const auto cameraId: selectedCameras)
                    selectedResources.append(data.allCameras.value(cameraId));

                q->ui->filteredResourceSelectionWidget->setInvalidMessage(
                    getText(selectedResources, true));
            }
            break;
        case Qt::Unchecked:
            selectedCameras.remove(resourceId);
            if (data.selectedInvalidCameras.remove(resourceId)
                && data.selectedInvalidCameras.isEmpty())
            {
                setLockCurrentMode(false);
                q->ui->filteredResourceSelectionWidget->clearInvalidMessage();
            }
            break;
        default:
            NX_ASSERT(false, "Should not get here!");
            break;

    }

}

void CameraSelectionDialog::Private::reloadViewData()
{
    const auto view = q->ui->filteredResourceSelectionWidget->view();

    if (view->state().rootNode)
        view->applyPatch(NodeViewStatePatch::clearNodeView());

    const auto root = createCameraNodes(data, isAdminUser, showInvalidCameras, q->resourcePool());
    view->applyPatch(NodeViewStatePatch::fromRootNode(root));
    view->setLeafResourcesSelected(selectedCameras, true);

    view->expandToDepth(0);
}

bool CameraSelectionDialog::Private::setShowInvalidCameras(bool value)
{
    if (showInvalidCameras == value)
        return false;

    showInvalidCameras = value;
    q->ui->allCamerasSwitch->setChecked(value);
    reloadViewData();
    return true;
}

void CameraSelectionDialog::Private::setLockCurrentMode(bool lock)
{
    q->ui->allCamerasSwitch->setEnabled(!lock);
}

//-------------------------------------------------------------------------------------------------

CameraSelectionDialog::CameraSelectionDialog(
    const ValidResourceCheck& validResourceCheck,
    const GetText& getText,
    const QnUuidSet& selectedCameras,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(this, validResourceCheck, getText, selectedCameras)),
    ui(new Ui::CameraSelectionDialog())
{
    ui->setupUi(this);
//    ui->filteredResourceSelectionWidget->setDetailsVisible(true);
    const auto view = ui->filteredResourceSelectionWidget->view();
    view->setupHeader();
    view->sortByColumn(ResourceNodeViewColumn::resourceNameColumn, Qt::AscendingOrder);
    connect(view, &ResourceSelectionNodeView::resourceSelectionChanged,
        d, &Private::handleSelectionChanged);

    connect(ui->allCamerasSwitch, &QPushButton::toggled, d, &Private::setShowInvalidCameras);

    d->setLockCurrentMode(
        !d->data.selectedInvalidCameras.isEmpty()   //< We have selected invalid cameras.
        || d->data.invalidCameras.isEmpty());       //< We have no invalid cameras.
    if (!d->setShowInvalidCameras(!d->data.selectedInvalidCameras.isEmpty()))
        d->reloadViewData();

    ui->filteredResourceSelectionWidget->view()->setMouseTracking(true);
    connect(ui->filteredResourceSelectionWidget->view(), &QAbstractItemView::entered,
        this, &CameraSelectionDialog::updateThumbnail);
}

CameraSelectionDialog::~CameraSelectionDialog()
{
}

bool CameraSelectionDialog::selectCamerasInternal(
    const ValidResourceCheck& validResourceCheck,
    const GetText& getText,
    QnUuidSet& selectedCameras,
    QWidget* parent)
{
    CameraSelectionDialog dialog(validResourceCheck, getText, selectedCameras, parent);

    if (dialog.d->data.allCameras.isEmpty())
    {
        QnMessageBox::warning(parent, tr("You do not have any cameras"));
        return false;
    }

    if (dialog.exec() != QDialog::Accepted)
        return false;

    selectedCameras = dialog.d->selectedCameras;
    return true;
}

void CameraSelectionDialog::updateThumbnail(const QModelIndex& index)
{
    QModelIndex baseIndex = index.sibling(index.row(), ResourceNodeViewColumn::resourceNameColumn);
    QString toolTip = baseIndex.data(Qt::DisplayRole).toString();
    ui->detailsWidget->setName(toolTip);
    ui->detailsWidget->setResource(
        baseIndex.data(ResourceNodeDataRole::resourceRole).value<QnResourcePtr>());
}

} // namespace nx::vms::client::desktop
