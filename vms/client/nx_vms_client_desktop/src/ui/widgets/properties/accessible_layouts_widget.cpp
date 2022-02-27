// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "accessible_layouts_widget.h"

#include "ui_accessible_layouts_widget.h"

#include <nx/utils/log/assert.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <ui/models/abstract_permissions_model.h>

#include <ui/delegates/resource_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_selection_widget.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_dialogs/details/resource_details_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>

using namespace  nx::vms::client::desktop;

namespace {

entity_item_model::AbstractEntityPtr createTreeEntity(
    const entity_resource_tree::ResourceTreeEntityBuilder* builder)
{
    return builder->createDialogShareableLayoutsEntity();
}

} // namespace

QnAccessibleLayoutsWidget::QnAccessibleLayoutsWidget(
    QnAbstractPermissionsModel* permissionsModel,
    QWidget* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::AccessibleLayoutsWidget()),
    m_permissionsModel(permissionsModel)
{
    NX_ASSERT(parent);

    ui->setupUi(this);

    using ResourceSelectionWidget = nx::vms::client::desktop::ResourceSelectionWidget;
    m_resourceSelectionWidget = new ResourceSelectionWidget(
        ResourceSelectionWidget::ResourceValidator(),
        ResourceSelectionWidget::AlertTextProvider(),
        this);
    m_resourceSelectionWidget->setTreeEntityFactoryFunction(createTreeEntity);
    ui->widgetLayout->addWidget(m_resourceSelectionWidget);

    m_resourceSelectionWidget->resourceViewWidget()->setUseTreeIndentation(false);
    m_resourceSelectionWidget->setDisplayResourceStatus(false);

    connect(m_resourceSelectionWidget, &ResourceSelectionWidget::selectionChanged,
        this, &QnAccessibleLayoutsWidget::hasChangesChanged);

    m_resourceSelectionWidget->detailsPanelWidget()->setDescriptionText(
        tr("Giving access to some layouts you give access to all cameras on them. "
            "Also user will get access to all new cameras on these layouts."));
}

QnAccessibleLayoutsWidget::~QnAccessibleLayoutsWidget()
{
}

bool QnAccessibleLayoutsWidget::hasChanges() const
{
    return m_resourceSelectionWidget->selectedResourcesIds() !=
        QnResourceAccessFilter::filteredResources(
            resourcePool(),
            QnResourceAccessFilter::LayoutsFilter,
            m_permissionsModel->accessibleResources());
}

void QnAccessibleLayoutsWidget::loadDataToUi()
{
    const auto accessibleLayoutResources = QnResourceAccessFilter::filteredResources(
        resourcePool(),
        QnResourceAccessFilter::LayoutsFilter,
        m_permissionsModel->accessibleResources());

    m_resourceSelectionWidget->setSelectedResourcesIds(accessibleLayoutResources);
}

QSet<QnUuid> QnAccessibleLayoutsWidget::checkedLayouts() const
{
    return m_resourceSelectionWidget->selectedResourcesIds();
}

void QnAccessibleLayoutsWidget::applyChanges()
{
    auto accessibleResources = m_permissionsModel->accessibleResources();
    auto oldFiltered = QnResourceAccessFilter::filteredResources(
        resourcePool(),
        QnResourceAccessFilter::LayoutsFilter,
        accessibleResources);
    auto newFiltered = checkedLayouts();

    accessibleResources.subtract(oldFiltered);
    accessibleResources.unite(newFiltered);

    m_permissionsModel->setAccessibleResources(accessibleResources);
}

int QnAccessibleLayoutsWidget::layoutsCount() const
{
    return m_resourceSelectionWidget->resourceCount();
}
