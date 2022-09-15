// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "detailed_resource_tree_widget.h"

#include "ui_detailed_resource_tree_widget.h"

#include <QtCore/QModelIndex>

#include <core/resource/resource.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/delegates/resource_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>
#include <nx/vms/client/desktop/common/models/item_model_algorithm.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_icon_decorator_model.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>

namespace nx::vms::client::desktop {

using NodeType = nx::vms::client::desktop::ResourceTree::NodeType;

DetailedResourceTreeWidget::DetailedResourceTreeWidget(
    int columnCount,
    QWidget* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::DetailedResourceTreeWidget()),
    m_treeEntityBuilder(new entity_resource_tree::ResourceTreeEntityBuilder(systemContext())),
    m_entityModel(new entity_item_model::EntityItemModel(columnCount)),
    m_iconDecoratorModel(new ResourceTreeIconDecoratorModel())
{
    ui->setupUi(this);

    connect(accessController(), &QnWorkbenchAccessController::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            if (!m_entityFactoryFunction)
                return;

            m_treeEntityBuilder->setUser(user);
            auto newTreeEntity = m_entityFactoryFunction(m_treeEntityBuilder.get());
            m_entityModel->setRootEntity(newTreeEntity.get());
            m_treeEntity = std::move(newTreeEntity);
        });

    connect(resourceViewWidget(), &FilteredResourceViewWidget::itemHovered,
        this, &DetailedResourceTreeWidget::updateDetailsPanel);

    m_treeEntityBuilder->setUser(accessController()->user());
    m_iconDecoratorModel->setSourceModel(m_entityModel.get());

    resourceViewWidget()->setItemDelegate(new ResourceDialogItemDelegate());
}

DetailedResourceTreeWidget::~DetailedResourceTreeWidget()
{
    m_entityModel->setRootEntity({});
}

void DetailedResourceTreeWidget::setTreeEntityFactoryFunction(
    const EntityFactoryFunction& entityFactoryFunction)
{
    m_entityFactoryFunction = entityFactoryFunction;
    if (m_entityFactoryFunction)
    {
        auto newTreeEntity = m_entityFactoryFunction(m_treeEntityBuilder.get());
        m_entityModel->setRootEntity(newTreeEntity.get());
        m_treeEntity = std::move(newTreeEntity);
    }
    else
    {
        m_entityModel->setRootEntity({});
        m_treeEntity.reset();
    }
}

bool DetailedResourceTreeWidget::displayResourceStatus() const
{
    return m_iconDecoratorModel->displayResourceStatus();
}

void DetailedResourceTreeWidget::setDisplayResourceStatus(bool display)
{
    m_iconDecoratorModel->setDisplayResourceStatus(display);
}

FilteredResourceViewWidget* DetailedResourceTreeWidget::resourceViewWidget() const
{
    return ui->filteredResourceSelectionWidget;
}

void DetailedResourceTreeWidget::setDetailsPanelUpdateFunction(
    const DetailsPanelUpdateFunction& updateFunction)
{
    m_detailsPanelUpdateFunction = updateFunction;
}

ResourceDetailsWidget* DetailedResourceTreeWidget::detailsPanelWidget() const
{
    return ui->detailsWidget;
}

void DetailedResourceTreeWidget::updateDetailsPanel(const QModelIndex& index)
{
    if (m_detailsPanelUpdateFunction)
    {
        m_detailsPanelUpdateFunction(index, detailsPanelWidget());
    }
    else
    {
        if (!index.isValid())
        {
            ui->detailsWidget->setCaptionText({});
            ui->detailsWidget->setThumbnailCameraResource({});
            return;
        }

        const auto resourceIndex = index.siblingAtColumn(0);
        ui->detailsWidget->setCaptionText(resourceIndex.data(Qt::DisplayRole).toString());
        ui->detailsWidget->setThumbnailCameraResource(
            resourceIndex.data(Qn::ResourceRole).value<QnResourcePtr>());
    }
}

bool DetailedResourceTreeWidget::isEmpty() const
{
    return m_entityModel->rowCount() == 0;
}

int DetailedResourceTreeWidget::resourceCount()
{
    const auto leafIndexes = item_model::getLeafIndexes(m_entityModel.get(), QModelIndex());
    QSet<QnResourcePtr> resourcesSet;
    resourcesSet.reserve(leafIndexes.size());
    for (const auto& leafIndex: leafIndexes)
    {
        const auto resourceData = leafIndex.data(Qn::ResourceRole);
        if (resourceData.isNull())
            continue;

        if (const auto resource = resourceData.value<QnResourcePtr>())
            resourcesSet.insert(resource);
    }
    return resourcesSet.size();
}

QAbstractItemModel* DetailedResourceTreeWidget::model() const
{
    return m_iconDecoratorModel.get();
}

void DetailedResourceTreeWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);

    if (!resourceViewWidget()->sourceModel())
    {
        resourceViewWidget()->setSourceModel(model());
        setupHeader();
    }

    ui->filteredResourceSelectionWidget->makeRequiredItemsVisible();
}

void DetailedResourceTreeWidget::setupHeader()
{
}

bool DetailedResourceTreeWidget::isDetailsPanelHidden() const
{
    return ui->detailsContainer->isHidden();
}

void DetailedResourceTreeWidget::setDetailsPanelHidden(bool value)
{
    ui->detailsContainer->setHidden(value);
}

} // namespace nx::vms::client::desktop
