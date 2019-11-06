#include "server_selection_dialog.h"
#include "ui_server_selection_dialog.h"

#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_selection_node_view.h>
#include <nx/vms/client/desktop/common/widgets/message_bar.h>

#include <nx_ec/access_helpers.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/resource_access_subject.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::node_view;
using namespace nx::vms::client::desktop::node_view::details;

QnMediaServerResourceList getAccessibleServers(
    const QnUserResourcePtr& currentUser,
    QnCommonModule* commonModule)
{
    using namespace ec2::access_helpers;

    const auto resourcePool = commonModule->resourcePool();
    const auto accessProvider = commonModule->resourceAccessProvider();

    return getAccessibleResources(
        currentUser, resourcePool->getResources<QnMediaServerResource>(), accessProvider);
}

NodePtr createServersRoot(
    const QnMediaServerResourceList& servers,
    const QnResourcePool* pool)
{
    const auto root = ViewNode::create();
    for (const auto& server: servers)
    {
        const auto camerasOnServer = pool->getAllCameras(server, true);
        const auto extraText = lit("- %1").arg(
            ServerSelectionDialog::tr("%n cameras", nullptr, camerasOnServer.size()));
        root->addChild(createResourceNode(server, extraText, pool));
    }
    return root;
}

} // namespace

namespace nx::vms::client::desktop {

struct ServerSelectionDialog::Private: public QObject
{
    Private(
        const ServerSelectionDialog* owner,
        const QnUuidSet& selectedServersIds,
        ServerSelectionDialog::ServerFilter filterFunctor);

    void handleSelectionChanged(const QnUuid& resourceId, Qt::CheckState checkedState);
    void reloadViewData();

    const ServerSelectionDialog* q = nullptr;
    const QnUserResourcePtr currentUser;
    QnUuidSet selectedServersIds;
    ServerSelectionDialog::ServerFilter filterFunctor;
};

ServerSelectionDialog::Private::Private(
    const ServerSelectionDialog* owner,
    const QnUuidSet& selectedServers,
    ServerSelectionDialog::ServerFilter filterFunctor)
    :
    q(owner),
    currentUser(q->context()->user()),
    selectedServersIds(selectedServers),
    filterFunctor(filterFunctor)
{
}

void ServerSelectionDialog::Private::handleSelectionChanged(
    const QnUuid& resourceId,
    Qt::CheckState checkedState)
{
    switch (checkedState)
    {
        case Qt::Checked:
            selectedServersIds.insert(resourceId);
            break;
        case Qt::Unchecked:
            selectedServersIds.remove(resourceId);
            break;
        default:
            NX_ASSERT(false, "Should not get here!");
            break;
    }
}

void ServerSelectionDialog::Private::reloadViewData()
{
    const auto view =
        qobject_cast<ResourceSelectionNodeView*>(q->ui->filteredResourceSelectionWidget->view());

    auto servers = getAccessibleServers(currentUser, q->commonModule());
    if (filterFunctor)
        servers = servers.filtered(filterFunctor);

    const auto resourcePool = q->resourcePool();

    if (view->state().rootNode)
        view->applyPatch(NodeViewStatePatch::clearNodeView());
    view->applyPatch(NodeViewStatePatch::fromRootNode(createServersRoot(servers, resourcePool)));

    view->setLeafResourcesSelected(selectedServersIds, true);
}

bool ServerSelectionDialog::selectServers(
    QnUuidSet& selectedServers,
    ServerFilter filterFunctor,
    const QString& infoMessage,
    QWidget* parent)
{
    ServerSelectionDialog dialog(selectedServers, filterFunctor, infoMessage, parent);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    selectedServers = dialog.d->selectedServersIds;
    return true;
}

ServerSelectionDialog::ServerSelectionDialog(
    const QnUuidSet& selectedServers,
    ServerFilter filterFunctor,
    const QString& infoMessage,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(this, selectedServers, filterFunctor)),
    ui(new Ui::ServerSelectionDialog())
{
    ui->setupUi(this);
    ui->filteredResourceSelectionWidget->setInfoMessage(infoMessage);

    static constexpr int kPlainListIndentation = 0;
    const auto view = ui->filteredResourceSelectionWidget->view();
    view->setupHeader();
    view->setIndentation(kPlainListIndentation);
    view->sortByColumn(ResourceNodeViewColumn::resourceNameColumn, Qt::AscendingOrder);

    connect(view, &ResourceSelectionNodeView::resourceSelectionChanged,
        d, &Private::handleSelectionChanged);

    d->reloadViewData();
}

ServerSelectionDialog::~ServerSelectionDialog()
{
}

} // namespace nx::vms::client::desktop
