// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "detailed_resource_tree_widget.h"
#include "ui_detailed_resource_tree_widget.h"

#include <QtCore/QModelIndex>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/common/models/item_model_algorithm.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_icon_decorator_model.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

namespace {

void defaultDetailsPanelUpdateFunction(
    const QModelIndex& index,
    ResourceDetailsWidget* detailsWidget)
{
    static constexpr int kResourceColumn = 0;

    if (!index.isValid())
    {
        detailsWidget->clear();
        return;
    }

    const auto resourceIndex = index.siblingAtColumn(kResourceColumn);
    detailsWidget->setCaption(resourceIndex.data(Qt::DisplayRole).toString());
    detailsWidget->setThumbnailCameraResource(
        resourceIndex.data(core::ResourceRole).value<QnResourcePtr>());
}

bool isIndexWithinRange(
    const QModelIndex& index,
    const QModelIndex& topLeft,
    const QModelIndex& bottomRight)
{
    if (!index.isValid())
        return false;

    if (!NX_ASSERT(topLeft.isValid() && bottomRight.isValid()))
        return false;

    if (!NX_ASSERT(index.model() == topLeft.model()))
        return false;

    if (index.parent() != topLeft.parent())
        return false;

    if (index.column() < topLeft.column() || index.column() > bottomRight.column())
        return false;

    if (index.row() < topLeft.row() || index.row() > bottomRight.row())
        return false;

    return true;
}

} //namespace

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
    m_iconDecoratorModel(new ResourceTreeIconDecoratorModel()),
    m_detailsPanelUpdateFunction(defaultDetailsPanelUpdateFunction)
{
    ui->setupUi(this);

    connect(accessController(), &AccessController::userChanged, this,
        [this]()
        {
            if (!m_entityFactoryFunction)
                return;

            m_treeEntityBuilder->setUser(accessController()->user());
            auto newTreeEntity = m_entityFactoryFunction(m_treeEntityBuilder.get());
            m_entityModel->setRootEntity(newTreeEntity.get());
            m_treeEntity = std::move(newTreeEntity);
        });

    connect(resourceViewWidget()->itemViewHoverTracker(), &ItemViewHoverTracker::itemEnter,
        this, &DetailedResourceTreeWidget::updateDetailsPanel);

    connect(resourceViewWidget()->itemViewHoverTracker(), &ItemViewHoverTracker::itemLeave,
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

void DetailedResourceTreeWidget::updateDetailsPanel()
{
    if (isDetailsPanelHidden())
        return;

    if (m_detailsPanelUpdateFunction)
    {
        detailsPanelWidget()->setUpdatesEnabled(false);

        const auto sourceIndex = resourceViewWidget()->toSourceIndex(
            resourceViewWidget()->itemViewHoverTracker()->hoveredIndex());
        m_detailsPanelUpdateFunction(sourceIndex, detailsPanelWidget());

        detailsPanelWidget()->setUpdatesEnabled(true);
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
        const auto resourceData = leafIndex.data(core::ResourceRole);
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
        connect(model(), &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex& topLeft, const QModelIndex& bottomRight)
            {
                const auto sourceIndex = resourceViewWidget()->toSourceIndex(
                    resourceViewWidget()->itemViewHoverTracker()->hoveredIndex());
                if (isIndexWithinRange(sourceIndex, topLeft, bottomRight))
                    updateDetailsPanel();
            });
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

void DetailedResourceTreeWidget::setDetailsPanelHidden(bool hide)
{
    if (isDetailsPanelHidden() == hide)
        return;

    ui->detailsContainer->setHidden(hide);

    if (hide)
        ui->detailsWidget->clear();
    else
        updateDetailsPanel();
}

} // namespace nx::vms::client::desktop
