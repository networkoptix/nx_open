// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multiple_layout_selection_dialog.h"

#include "ui_multiple_layout_selection_dialog.h"

#include <QtWidgets/QHeaderView>

#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/checkbox_column_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_decorator_model.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_icon_decorator_model.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

using namespace entity_item_model;
using namespace entity_resource_tree;

struct MultipleLayoutSelectionDialog::Private: public QObject
{
    Private(const MultipleLayoutSelectionDialog* owner, const QnUuidSet& selectedLayoutsIds);
    ~Private();

    const MultipleLayoutSelectionDialog* q = nullptr;
    const std::unique_ptr<entity_resource_tree::ResourceTreeEntityBuilder> treeEntityBuilder;
    AbstractEntityPtr layoutsEntity;
    const std::unique_ptr<EntityItemModel> layoutsModel;
    const std::unique_ptr<ResourceTreeIconDecoratorModel> iconDecoratorModel;
    std::unique_ptr<ResourceSelectionDecoratorModel> selectionDecoratorModel;
};

MultipleLayoutSelectionDialog::Private::Private(
    const MultipleLayoutSelectionDialog* owner,
    const QnUuidSet& selectedLayoutsIds)
    :
    q(owner),
    treeEntityBuilder(new ResourceTreeEntityBuilder(q->commonModule())),
    layoutsModel(new EntityItemModel(resource_selection_view::ColumnCount)),
    iconDecoratorModel(new ResourceTreeIconDecoratorModel())
{
    treeEntityBuilder->setUser(q->context()->accessController()->user());
    layoutsEntity = treeEntityBuilder->createDialogAllLayoutsEntity();
    layoutsModel->setRootEntity(layoutsEntity.get());
    iconDecoratorModel->setSourceModel(layoutsModel.get());

    const auto selectedLayoutsList =
        q->commonModule()->resourcePool()->getResourcesByIds(selectedLayoutsIds);
    selectionDecoratorModel.reset(new ResourceSelectionDecoratorModel());
    selectionDecoratorModel->setSelectedResources(
        {std::cbegin(selectedLayoutsList), std::cend(selectedLayoutsList)});

    selectionDecoratorModel->setSourceModel(iconDecoratorModel.get());
}

MultipleLayoutSelectionDialog::Private::~Private()
{
    layoutsModel->setRootEntity(nullptr);
}

//-------------------------------------------------------------------------------------------------

bool MultipleLayoutSelectionDialog::selectLayouts(QnUuidSet& selectedLayoutsIds, QWidget* parent)
{
    MultipleLayoutSelectionDialog dialog(selectedLayoutsIds, parent);

    if (dialog.d->layoutsModel->rowCount() == 0)
    {
        QnMessageBox::warning(parent, tr("You do not have any layouts"));
        return false;
    }

    if (dialog.exec() != QDialog::Accepted)
        return false;

    selectedLayoutsIds = dialog.d->selectionDecoratorModel->selectedResourcesIds();
    return true;
}

MultipleLayoutSelectionDialog::MultipleLayoutSelectionDialog(
    const QnUuidSet& selectedLayouts,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(this, selectedLayouts)),
    ui(new Ui::MultipleLayoutSelectionDialog())
{
    using namespace resource_selection_view;

    ui->setupUi(this);

    ui->filteredResourceViewWidget->setSourceModel(d->selectionDecoratorModel.get());
    ui->filteredResourceViewWidget->setItemDelegateForColumn(ResourceColumn,
        new ResourceDialogItemDelegate(this));
    ui->filteredResourceViewWidget->setItemDelegateForColumn(CheckboxColumn,
        new CheckBoxColumnItemDelegate(this));
    ui->filteredResourceViewWidget->setUseTreeIndentation(false);

    static constexpr int kCheckboxColumnWidth =
        nx::style::Metrics::kDefaultTopLevelMargin + nx::style::Metrics::kViewRowHeight;

    const auto header = ui->filteredResourceViewWidget->treeHeaderView();
    header->setStretchLastSection(false);
    header->resizeSection(CheckboxColumn, kCheckboxColumnWidth);
    header->setSectionResizeMode(ResourceColumn, QHeaderView::ResizeMode::Stretch);
    header->setSectionResizeMode(CheckboxColumn, QHeaderView::ResizeMode::Fixed);

    connect(ui->filteredResourceViewWidget, &FilteredResourceViewWidget::itemClicked, this,
        [this](const QModelIndex& index)
        {
            d->selectionDecoratorModel->toggleSelection(index.siblingAtColumn(ResourceColumn));
        });
}

MultipleLayoutSelectionDialog::~MultipleLayoutSelectionDialog()
{
}

} // namespace nx::vms::client::desktop
