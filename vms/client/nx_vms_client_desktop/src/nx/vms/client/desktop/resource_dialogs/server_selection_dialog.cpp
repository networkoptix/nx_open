// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_selection_dialog.h"
#include "ui_server_selection_dialog.h"

#include <functional>

#include <QtWidgets/QHeaderView>

#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity/unique_key_list_entity.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item_order/item_order.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/checkbox_column_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_icon_decorator_model.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::client::core;
using namespace nx::vms::client::core::entity_item_model;

namespace {

AbstractItemPtr createServerItem(const QnResourcePtr& serverResource)
{
    const auto resourcePool = serverResource->resourcePool();
    if (!resourcePool)
        return {};

    const auto camerasOnServer = resourcePool->getAllCameras(serverResource, true);
    const auto extraText = QString("- %1").arg(
        ServerSelectionDialog::tr("%n cameras", nullptr, camerasOnServer.size()));

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, serverResource->getName())
        .withRole(ResourceIconKeyRole, static_cast<int>(ResourceIconCache::Server))
        .withRole(ResourceRole, QVariant::fromValue(serverResource))
        .withRole(ExtraInfoRole, extraText)
        .withFlags({Qt::ItemNeverHasChildren, Qt::ItemIsEnabled});
}

std::function<AbstractItemPtr(const QnResourcePtr&)> serverItemCreator()
{
    return
        [](const QnResourcePtr& serverResource) -> AbstractItemPtr
        {
            if (serverResource.isNull() || !serverResource->hasFlags(Qn::server))
                return entity_item_model::AbstractItemPtr();

            return createServerItem(serverResource);
        };
}

QnMediaServerResourceList getAccessibleServers(
    const QnUserResourcePtr& currentUser)
{
    const auto systemContext = nx::vms::client::core::SystemContext::fromResource(currentUser);
    const auto accessManager = systemContext->resourceAccessManager();

    return systemContext->resourcePool()->servers().filtered(
        [currentUser, accessManager](const QnResourcePtr& resource) -> bool
        {
            return accessManager->hasPermission(currentUser, resource, Qn::ViewContentPermission);
        });
}

AbstractEntityPtr createServersEntity(
    const QnMediaServerResourceList& servers,
    const QnResourcePool* /*resourcePool*/)
{
    auto result = makeKeyList<QnResourcePtr>(serverItemCreator(), numericOrder());
    for (const auto& server: servers)
        result->addItem(server);
    return result;
}

} // namespace

struct ServerSelectionDialog::Private:
    public QObject,
    public SystemContextAware
{
    Private(
        const ServerSelectionDialog* owner,
        const UuidSet& selectedServersIds,
        ServerSelectionDialog::ServerFilter filterFunctor,
        bool multiSelect);
    ~Private();

    void onItemClicked(const QModelIndex& index);

    const ServerSelectionDialog* q = nullptr;
    UuidSet selectedServersIds;
    ServerSelectionDialog::ServerFilter filterFunctor;

    AbstractEntityPtr rootEntity;
    const std::unique_ptr<EntityItemModel> entityModel;
    const std::unique_ptr<ResourceTreeIconDecoratorModel> iconDecoratorModel;
    std::unique_ptr<ResourceSelectionDecoratorModel> selectionDecoratorModel;
};

ServerSelectionDialog::Private::Private(
    const ServerSelectionDialog* owner,
    const UuidSet& selectedServersIds,
    ServerSelectionDialog::ServerFilter filterFunctor,
    bool multiSelect)
    :
    SystemContextAware(owner->system()),
    q(owner),
    filterFunctor(filterFunctor),
    entityModel(new EntityItemModel(resource_selection_view::ColumnCount)),
    iconDecoratorModel(new ResourceTreeIconDecoratorModel())

{
    const auto serversList = resourcePool()->getResourcesByIds(selectedServersIds);

    selectionDecoratorModel.reset(new ResourceSelectionDecoratorModel());
    if (!multiSelect && serversList.empty())
        selectionDecoratorModel->setPinnedItemSelected(true);
    else
        selectionDecoratorModel->setSelectedResources(nx::utils::toQSet(serversList));

    iconDecoratorModel->setSourceModel(entityModel.get());
    selectionDecoratorModel->setSourceModel(iconDecoratorModel.get());

    auto serversEntity = createServersEntity(getAccessibleServers(systemContext()->user())
        .filtered(filterFunctor), resourcePool());

    if (multiSelect)
    {
        rootEntity = std::move(serversEntity);
    }
    else
    {
        selectionDecoratorModel->setSelectionMode(ResourceSelectionMode::ExclusiveSelection);

        AbstractItemPtr pinnedItem = GenericItemBuilder()
            .withRole(core::ResourceIconKeyRole, (int) core::ResourceIconCache::Server)
            .withRole(Qt::DisplayRole, ServerSelectionDialog::tr("Source Server"))
            .withRole(ResourceDialogItemRole::IsPinnedItemRole, true)
            .withFlags({Qt::ItemIsEnabled, Qt::ItemNeverHasChildren});

        desktop::entity_resource_tree::ResourceTreeEntityBuilder builder(systemContext());
        rootEntity = builder.addPinnedItem(
            std::move(serversEntity), std::move(pinnedItem), /*withSeparator*/ false);
    }

    entityModel->setRootEntity(rootEntity.get());

    connect(q->ui->filteredResourceSelectionWidget, &FilteredResourceViewWidget::itemClicked,
        this, &ServerSelectionDialog::Private::onItemClicked);
}

ServerSelectionDialog::Private::~Private()
{
    entityModel->setRootEntity(nullptr);
}

void ServerSelectionDialog::Private::onItemClicked(const QModelIndex& index)
{
    selectionDecoratorModel->toggleSelection(
        index.siblingAtColumn(resource_selection_view::ResourceColumn));
}

//-------------------------------------------------------------------------------------------------

bool ServerSelectionDialog::selectServers(
    UuidSet& selectedServers,
    ServerFilter filterFunctor,
    const QString& infoMessage,
    QWidget* parent)
{
    ServerSelectionDialog dialog(
        selectedServers, filterFunctor, infoMessage,  /*multiSelect*/ true, parent);

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

void ServerSelectionDialog::selectServer(
    nx::Uuid selectedServer,
    ServerFilter filterFunctor,
    const QString& infoMessage,
    SelectServerCallback callback,
    QWidget* parent)
{
    auto dialog = new ServerSelectionDialog(
        UuidSet{selectedServer}, filterFunctor, infoMessage, /*multiSelect*/ false, parent);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::ApplicationModal);

    QObject::connect(
        dialog,
        &QDialog::finished,
        dialog,
        [dialog, callback](int result)
        {
            const auto ids = dialog->d->selectionDecoratorModel->selectedResourcesIds();
            const auto selectedServer = ids.empty() ? nx::Uuid() : *ids.cbegin();

            callback(result == QDialog::Accepted, selectedServer);
        });

    dialog->show();
}

ServerSelectionDialog::ServerSelectionDialog(
    const UuidSet& selectedServers,
    ServerFilter filterFunctor,
    const QString& infoMessage,
    bool multiSelect,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::ServerSelectionDialog())
{
    using namespace resource_selection_view;

    ui->setupUi(this);

    d.reset(new Private(this, selectedServers, filterFunctor, multiSelect));

    ui->filteredResourceSelectionWidget->setInfoMessage(infoMessage);

    ui->filteredResourceSelectionWidget->setSourceModel(d->selectionDecoratorModel.get());
    ui->filteredResourceSelectionWidget->setItemDelegateForColumn(ResourceColumn,
        new ResourceDialogItemDelegate(this));

    auto selectionDelegate = new CheckBoxColumnItemDelegate(this);
    if (!multiSelect)
    {
        setWindowTitle(tr("Select Server"));
        selectionDelegate->setCheckMarkType(
            CheckBoxColumnItemDelegate::CheckMarkType::RadioButton);
    }

    ui->filteredResourceSelectionWidget->setItemDelegateForColumn(
        CheckboxColumn, selectionDelegate);

    ui->filteredResourceSelectionWidget->setUseTreeIndentation(false);

    static constexpr int kCheckboxColumnWidth =
        nx::style::Metrics::kDefaultTopLevelMargin + nx::style::Metrics::kViewRowHeight;

    const auto header = ui->filteredResourceSelectionWidget->treeHeaderView();
    header->setStretchLastSection(false);
    header->resizeSection(CheckboxColumn, kCheckboxColumnWidth);
    header->setSectionResizeMode(ResourceColumn, QHeaderView::ResizeMode::Stretch);
    header->setSectionResizeMode(CheckboxColumn, QHeaderView::ResizeMode::Fixed);
}

ServerSelectionDialog::~ServerSelectionDialog()
{
}

bool ServerSelectionDialog::isEmpty() const
{
    return d->entityModel->rowCount() == 0;
}

} // namespace nx::vms::client::desktop
