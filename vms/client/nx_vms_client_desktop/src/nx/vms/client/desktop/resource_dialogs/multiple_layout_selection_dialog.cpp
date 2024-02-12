// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multiple_layout_selection_dialog.h"

#include "ui_multiple_layout_selection_dialog.h"

#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_selection_widget.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace entity_item_model;
using namespace entity_resource_tree;

AbstractEntityPtr createTreeEntity(const ResourceTreeEntityBuilder* builder)
{
    return builder->createDialogAllLayoutsEntity();
}

} // namespace

namespace nx::vms::client::desktop {

bool MultipleLayoutSelectionDialog::selectLayouts(UuidSet& selectedLayoutsIds, QWidget* parent)
{
    MultipleLayoutSelectionDialog dialog(selectedLayoutsIds, parent);

    if (dialog.m_resourceSelectionWidget->isEmpty())
    {
        QnMessageBox::warning(parent, tr("You do not have any layouts"));
        return false;
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        selectedLayoutsIds = dialog.m_resourceSelectionWidget->selectedResourcesIds();
        return true;
    }

    return false;
}

MultipleLayoutSelectionDialog::MultipleLayoutSelectionDialog(
    const UuidSet& selectedLayoutsIds,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::MultipleLayoutSelectionDialog()),
    m_resourceSelectionWidget(new ResourceSelectionWidget(
        this, resource_selection_view::ColumnCount))
{
    ui->setupUi(this);
    ui->widgetLayout->insertWidget(0, m_resourceSelectionWidget);
    m_resourceSelectionWidget->setDetailsPanelHidden(true);
    m_resourceSelectionWidget->resourceViewWidget()->setUseTreeIndentation(false);
    m_resourceSelectionWidget->setTreeEntityFactoryFunction(createTreeEntity);
    m_resourceSelectionWidget->setSelectedResourcesIds(selectedLayoutsIds);
}

MultipleLayoutSelectionDialog::~MultipleLayoutSelectionDialog()
{
}

} // namespace nx::vms::client::desktop
