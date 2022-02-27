// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_selection_dialog.h"
#include "ui_server_selection_dialog.h"

#include <functional>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_icon_decorator_model.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_selection_dialog_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::entity_item_model;

AbstractItemPtr createServerItem(const QnResourcePtr& serverResource)
{
    const auto resourcePool = serverResource->resourcePool();
    if (!resourcePool)
        return {};

    const auto camerasOnServer = resourcePool->getAllCameras(serverResource, true);
    const auto extraText = QString("- %1").arg(
        ServerSelectionDialog::tr("%n cameras", nullptr, camerasOnServer.size()));

    return entity_item_model::GenericItemBuilder()
        .withRole(Qt::DisplayRole, serverResource->getName())
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(QnResourceIconCache::Server))
        .withRole(Qn::ResourceRole, QVariant::fromValue(serverResource))
        .withRole(Qn::ExtraInfoRole, extraText)
        .withFlags({Qt::ItemNeverHasChildren, Qt::ItemIsEnabled});
}

std::function<AbstractItemPtr(const QnResourcePtr&)> serverItemCreator()
{
    return
        [](const QnResourcePtr& serverResource) -> AbstractItemPtr
        {
            if (serverResource.isNull()
                || !serverResource->hasFlags(Qn::server)
                || serverResource->hasFlags(Qn::fake_server))
            {
                return entity_item_model::AbstractItemPtr();
            }
            return createServerItem(serverResource);
        };
}

QnMediaServerResourceList getAccessibleServers(
    const QnUserResourcePtr& currentUser,
    QnCommonModule* commonModule)
{
    using namespace nx::core::access;

    const auto resourcePool = commonModule->resourcePool();
    const auto accessProvider = commonModule->resourceAccessProvider();

    return getAccessibleResources(currentUser,
        resourcePool->getResources<QnMediaServerResource>(), accessProvider);
}

AbstractEntityPtr createServersEntity(
    const QnMediaServerResourceList& servers,
    const QnResourcePool* resourcePool)
{
    auto result = makeKeyList<QnResourcePtr>(serverItemCreator(), numericOrder());
    for (const auto& server: servers)
        result->addItem(server);
    return result;
}

} // namespace

namespace nx::vms::client::desktop {

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::entity_item_model;

struct ServerSelectionDialog::Private: public QObject
{
    Private(
        const ServerSelectionDialog* owner,
        const QnUuidSet& selectedServersIds,
        ServerSelectionDialog::ServerFilter filterFunctor);
    ~Private();

    void onItemClicked(const QModelIndex& index);

    const ServerSelectionDialog* q = nullptr;
    const QnUserResourcePtr currentUser;
    QnUuidSet selectedServersIds;
    ServerSelectionDialog::ServerFilter filterFunctor;

    AbstractEntityPtr serversEntity;
    const std::unique_ptr<EntityItemModel> entityModel;
    const std::unique_ptr<ResourceTreeIconDecoratorModel> iconDecoratorModel;
    std::unique_ptr<ResourceSelectionDecoratorModel> selectionDecoratorModel;
};

ServerSelectionDialog::Private::Private(
    const ServerSelectionDialog* owner,
    const QnUuidSet& selectedServersIds,
    ServerSelectionDialog::ServerFilter filterFunctor)
    :
    q(owner),
    currentUser(q->context()->user()),
    filterFunctor(filterFunctor),
    entityModel(new EntityItemModel()),
    iconDecoratorModel(new ResourceTreeIconDecoratorModel())

{
    const auto serversList =
        q->commonModule()->resourcePool()->getResourcesByIds(selectedServersIds);

    selectionDecoratorModel.reset(new ResourceSelectionDecoratorModel());
    selectionDecoratorModel->setSelectedResources(
        {std::cbegin(serversList), std::cend(serversList)});

    iconDecoratorModel->setSourceModel(entityModel.get());
    selectionDecoratorModel->setSourceModel(iconDecoratorModel.get());

    serversEntity = createServersEntity(getAccessibleServers(currentUser, q->commonModule())
        .filtered(filterFunctor), q->resourcePool());
    entityModel->setRootEntity(serversEntity.get());

    connect(q->ui->filteredResourceSelectionWidget, &FilteredResourceViewWidget::itemClicked,
        this, &ServerSelectionDialog::Private::onItemClicked);
}

ServerSelectionDialog::Private::~Private()
{
    entityModel->setRootEntity(nullptr);
}

void ServerSelectionDialog::Private::onItemClicked(const QModelIndex& index)
{
    selectionDecoratorModel->toggleSelection(index);
}

bool ServerSelectionDialog::selectServers(
    QnUuidSet& selectedServers,
    ServerFilter filterFunctor,
    const QString& infoMessage,
    QWidget* parent)
{
    ServerSelectionDialog dialog(selectedServers, filterFunctor, infoMessage, parent);

    if (dialog.isEmpty())
    {
        QnMessageBox::warning(parent, tr("There are no suitable servers"));
        return false;
    }

    if (dialog.exec() != QDialog::Accepted)
        return false;

    selectedServers = dialog.d->selectionDecoratorModel->selectedResourcesIds();

    return true;
}

ServerSelectionDialog::ServerSelectionDialog(
    const QnUuidSet& selectedServers,
    ServerFilter filterFunctor,
    const QString& infoMessage,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::ServerSelectionDialog())
{
    ui->setupUi(this);

    d.reset(new Private(this, selectedServers, filterFunctor));

    ui->filteredResourceSelectionWidget->setInfoMessage(infoMessage);

    ui->filteredResourceSelectionWidget->setSourceModel(d->selectionDecoratorModel.get());
    ui->filteredResourceSelectionWidget->setItemDelegate(
        new ResourceSelectionDialogItemDelegate(this));

    ui->filteredResourceSelectionWidget->setUseTreeIndentation(false);
}

ServerSelectionDialog::~ServerSelectionDialog()
{
}

bool ServerSelectionDialog::isEmpty() const
{
    return d->entityModel->rowCount() == 0;
}

} // namespace nx::vms::client::desktop
