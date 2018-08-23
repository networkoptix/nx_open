#include "camera_selection_dialog.h"

#include "ui_camera_selection_dialog.h"
#include "details/filtered_resource_selection_widget.h"

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <nx_ec/access_helpers.h>
#include <nx/client/desktop/node_view/resource_node_view/resource_selection_node_view.h>
#include <nx/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>

namespace {

using namespace nx::client::desktop;
using namespace node_view;
using namespace details;


using CameraValidityHash = QHash<QnUuid, bool>; //< Camera id and valid state data.
using ServerCameraHash = QHash<QnUuid, CameraValidityHash>;

ServerCameraHash createCamerasData(
    const QnUserResourcePtr& currentUser,
    const QnCommonModule* commonModule,
    const ValidResourceCheck& validCheck)
{
    ServerCameraHash result;

    using namespace ec2::access_helpers;
    const auto pool = commonModule->resourcePool();
    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto accessibleCameras = getAccessibleResources(
        currentUser, pool->getResources<QnVirtualCameraResource>(), accessProvider);
    for (const auto camera: accessibleCameras)
    {
        const auto parentServer = camera->getParentServer();
        const bool isValidResource = !validCheck || validCheck(camera);
        result[parentServer->getId()].insert(camera->getId(), isValidResource);
    }
    return result;
}

NodePtr createServerNode(
    const QnUuid& serverId,
    QnResourcePool* pool,
    const NodeList& children)
{
    const auto extraText = lit("- %1").arg(
        CameraSelectionDialog::tr("%n cameras", nullptr, children.size()));
    return createResourceNode(pool->getResourceById(serverId), extraText, false);
}

NodePtr createCameraNodes(
    const ServerCameraHash& data,
    bool adminMode,
    bool showInvalidCameras,
    QnResourcePool* pool)
{
    const NodePtr root = ViewNode::create();
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        NodeList children;
        const auto& cameras = it.value();
        for (auto itCamera = cameras.begin(); itCamera != cameras.end(); ++itCamera)
        {
            const auto cameraId = itCamera.key();
            const auto isValid = false;//itCamera.value();
            if (isValid || showInvalidCameras)
            {
                const auto cameraNode = createResourceNode(
                    pool->getResourceById(cameraId), QString(), true);
                setNodeValidState(cameraNode, isValid);
                children.append(cameraNode);
            }
        }

        if (children.isEmpty())
            continue;

        const NodePtr targetNode = adminMode ? createServerNode(it.key(), pool, children) : root;
        targetNode->addChildren(children);
        if (adminMode)
            root->addChild(targetNode);
    }

    return root;
}

bool hasAdminPermissions(const QnUserResourcePtr& user)
{
    return true;
}

} // namespace

namespace nx::client::desktop {

struct CameraSelectionDialog::Private: public QObject
{
    Private(
        CameraSelectionDialog* owner,
        const ValidResourceCheck& validResourceCheck,
        const node_view::details::UuidSet& selectedCameras);

    void setShowInvalidCameras(bool value);
    void reloadViewData();

    const CameraSelectionDialog* q;
    const QnUserResourcePtr currentUser;
    const bool isAdminUser;
    node_view::details::UuidSet selectedCameras;
    ServerCameraHash data;
    bool showInvalidCameras;
};

CameraSelectionDialog::Private::Private(
    CameraSelectionDialog* owner,
    const ValidResourceCheck& validResourceCheck,
    const node_view::details::UuidSet& selectedCameras)
    :
    q(owner),
    currentUser(q->commonModule()->instance<nx::client::core::UserWatcher>()->user()),
    isAdminUser(hasAdminPermissions(currentUser)),
    selectedCameras(selectedCameras),
    data(createCamerasData(currentUser, q->commonModule(), validResourceCheck))
{
}

void CameraSelectionDialog::Private::setShowInvalidCameras(bool value)
{
    if (showInvalidCameras == value)
        return;

    showInvalidCameras = value;
    reloadViewData();

}

void CameraSelectionDialog::Private::reloadViewData()
{
    const auto view = q->ui->filteredResourceSelectionWidget->resourceSelectionView();
    if (view->state().rootNode)
        view->applyPatch(NodeViewStatePatch::clearNodeView());
    view->applyPatch(NodeViewStatePatch::fromRootNode(createCameraNodes(
        data, isAdminUser, showInvalidCameras, q->resourcePool())));
    view->setLeafResourcesSelected(selectedCameras, true);

    view->expandAll();
}

//-------------------------------------------------------------------------------------------------

CameraSelectionDialog::CameraSelectionDialog(
    const ValidResourceCheck& validResourceCheck,
    const node_view::details::UuidSet& selectedCameras,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(this, validResourceCheck, selectedCameras)),
    ui(new Ui::CameraSelectionDialog())
{
    ui->setupUi(this);

    const auto tree = ui->filteredResourceSelectionWidget->resourceSelectionView();
    tree->setupHeader();

    connect(ui->allCamerasSwitch, &QPushButton::toggled, d, &Private::setShowInvalidCameras);

    d->reloadViewData();
}

CameraSelectionDialog::~CameraSelectionDialog()
{
}


bool CameraSelectionDialog::selectCamerasInternal(
    const ValidResourceCheck& validResourceCheck,
    node_view::details::UuidSet& selectedCameras,
    QWidget* parent)
{
    CameraSelectionDialog dialog(validResourceCheck, selectedCameras, parent);

    if (dialog.d->data.isEmpty())
    {
        QnMessageBox::warning(parent, tr("You do not have any cameras"));
        return false;
    }

    if (dialog.exec() != QDialog::Accepted)
        return false;

    selectedCameras = dialog.d->selectedCameras;
    return true;
}

} // namespace nx::client::desktop
