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
#include <nx_ec/access_helpers.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_selection_node_view.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/details/resource_node_view_item_delegate.h>
#include <client/client_settings.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/desktop/node_view/node_view/sorting_filtering/node_view_base_sort_model.h>

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
        const auto server = camera->getParentServer();
        if (!server)
            continue;

        const auto parentServerId = server->getId();
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
    bool showExtraInfo,
    const NodeList& children,
    QnResourcePool* pool)
{
    NodePtr result;
    if (auto server = pool->getResourceById(serverId))
    {
        const auto camerasCount = std::accumulate(children.cbegin(), children.cend(), 0,
            [](int count, const NodePtr& node)
            {
                return count + std::max(1, node->childrenCount());
            });
        QString extraText;
        if (showExtraInfo)
            extraText = QnResourceDisplayInfo(server).extraInfo();
        extraText = lit("%1 - %2").arg(
            extraText, CameraSelectionDialog::tr("%n cameras", nullptr, camerasCount)).trimmed();

        result = createResourceNode(server, extraText, true);
        if (result)
            result->addChildren(children);
    }
    return result;
}

QnVirtualCameraResourcePtr getGroupResource(const QString& groupId, QnResourcePool* resourcePool)
{
    auto groupResources = resourcePool->getResourcesBySharedId(groupId);
    if (!groupResources.isEmpty())
        return groupResources.first().dynamicCast<QnVirtualCameraResource>();
    return QnVirtualCameraResourcePtr();
}

NodePtr createCameraNode(
    const QnUuid& cameraId,
    bool showExtraInfo,
    bool isInvalidCamera,
    const QnResourcePool* pool)
{
    if (auto resource = pool->getResourceById(cameraId))
    {
        QString extraText;
        if (showExtraInfo)
            extraText = QnResourceDisplayInfo(resource).extraInfo();
        const auto cameraNode = createResourceNode(resource, extraText, true);
        if (isInvalidCamera)
            setNodeValidState(cameraNode, false);
        return cameraNode;
    }
    return NodePtr();
}

NodeList createCameraNodes(
    const QnUuidSet& camerasIds,
    bool showExtraInfo,
    bool showInvalidCameras,
    const Data& data,
    const QnResourcePool* pool)
{
    NodeList result;
    for (const auto& cameraId: camerasIds)
    {
        const bool isInvalidCamera = data.invalidCameras.contains(cameraId);
        if (showInvalidCameras || !isInvalidCamera)
        {
            if (auto cameraNode = createCameraNode(cameraId, showExtraInfo, isInvalidCamera, pool))
                result.append(cameraNode);
        }
    }
    return result;
}

NodePtr createCamerasTree(
    bool showInvalidCameras,
    const Data& data,
    QnResourcePool* pool)
{
    const NodePtr root = ViewNode::create();
    const bool showExtraInfo = qnSettings->extraInfoInTree() == Qn::RI_FullInfo;

    for (const auto& serverId: data.serverIds)
    {
        NodeList serverChildren;
        if (data.cameraGroupsByServerId.contains(serverId))
        {
            for (const auto& groupId: data.cameraGroupsByServerId.value(serverId))
            {
                if (auto groupResource = getGroupResource(groupId, pool))
                {
                    QString groupExtraText = showExtraInfo ?
                        QnResourceDisplayInfo(groupResource).extraInfo() : QString();
                    const auto groupNode = createGroupNode(groupResource, groupExtraText, true);

                    const auto groupChildrenIds = data.singleCamerasByGroupId.value(groupId);
                    NodeList groupChildren = createCameraNodes(
                        groupChildrenIds, showExtraInfo, showInvalidCameras, data, pool);
                    groupNode->addChildren(groupChildren);

                    if (groupNode->childrenCount())
                        serverChildren.append(groupNode);
                }
            }
        }

        const auto singleCameraIds = data.singleCamerasByServerId.value(serverId);
        serverChildren.append(
            createCameraNodes(singleCameraIds, showExtraInfo, showInvalidCameras, data, pool));

        if (serverChildren.isEmpty())
            continue;

        if (auto serverNode = createServerNode(serverId, showExtraInfo, serverChildren, pool))
            root->addChild(serverNode);
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
    void updateAlertMessage();

    const CameraSelectionDialog* q;
    const GetText getText;
    const QnUserResourcePtr currentUser;
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
            break;
        case Qt::Unchecked:
            selectedCameras.remove(resourceId);
            if (data.selectedInvalidCameras.remove(resourceId)
                && data.selectedInvalidCameras.isEmpty())
            {
                setLockCurrentMode(false);
            }
            break;
        default:
            NX_ASSERT(false, "Should not get here!");
            break;

    }
    updateAlertMessage();
}

void CameraSelectionDialog::Private::reloadViewData()
{
    const auto view = q->ui->filteredResourceSelectionWidget->view();

    if (const auto filterModel = qobject_cast<node_view::NodeViewBaseSortModel*>(view->model()))
        filterModel->setFilteringEnabled(false);

    if (view->state().rootNode)
        view->applyPatch(NodeViewStatePatch::clearNodeView());

    const auto root = createCamerasTree(showInvalidCameras, data, q->resourcePool());
    view->applyPatch(NodeViewStatePatch::fromRootNode(root));
    view->setLeafResourcesSelected(selectedCameras, true);

    view->expandToDepth(0);
    updateAlertMessage();

    if (const auto filterModel = qobject_cast<node_view::NodeViewBaseSortModel*>(view->model()))
        filterModel->setFilteringEnabled(true);
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

void CameraSelectionDialog::Private::updateAlertMessage()
{
    if (getText)
    {
        QnResourceList selectedResources;
        for (const auto cameraId: selectedCameras)
            selectedResources.append(data.allCameras.value(cameraId));

        q->ui->filteredResourceSelectionWidget->setInvalidMessage(
            getText(selectedResources, true));
    }
    else
    {
        q->ui->filteredResourceSelectionWidget->setInvalidMessage(QString());
    }
}

//-------------------------------------------------------------------------------------------------

CameraSelectionDialog::CameraSelectionDialog(
    const ValidResourceCheck& validResourceCheck,
    const GetText& getText,
    const QnUuidSet& selectedCameras,
    bool showRecordingIndicator,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(this, validResourceCheck, getText, selectedCameras)),
    ui(new Ui::CameraSelectionDialog())
{
    ui->setupUi(this);
    const auto view = ui->filteredResourceSelectionWidget->view();
    if (ResourceNodeViewItemDelegate* resourceDelegate =
        dynamic_cast<ResourceNodeViewItemDelegate*>(view->itemDelegate()))
    {
        resourceDelegate->setShowRecordingIndicator(showRecordingIndicator);
    }
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
    bool showRecordingIndicator,
    QWidget* parent)
{
    CameraSelectionDialog dialog(validResourceCheck, getText, selectedCameras,
        showRecordingIndicator, parent);

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
