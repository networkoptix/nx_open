// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_selection_widget.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHeaderView>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/checkbox_column_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/models/invalid_resource_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/models/invalid_resource_filter_model.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <ui/workbench/workbench_context.h>

namespace {

using namespace nx::vms::client::desktop;

CheckBoxColumnItemDelegate* selectionItemDelegate(
    const ResourceSelectionWidget* resourceSelectionWidget, int column)
{
    const auto result = dynamic_cast<CheckBoxColumnItemDelegate*>(
        resourceSelectionWidget->resourceViewWidget()->itemDelegateForColumn(column));

    NX_ASSERT(result, "Unexpected item delegate type");

    return result;
}

ResourceDialogItemDelegate* dialogItemDelegate(
    const ResourceSelectionWidget* resourceSelectionWidget)
{
    const auto result = dynamic_cast<ResourceDialogItemDelegate*>(
        resourceSelectionWidget->resourceViewWidget()->itemDelegate());

    NX_ASSERT(result, "Unexpected item delegate type");

    return result;
}

} // namespace

namespace nx::vms::client::desktop {

struct ResourceSelectionWidget::Private: public QObject
{
    Private(
        ResourceSelectionWidget* owner,
        int columnCount,
        const ResourceValidator& resourceValidator,
        const AlertTextProvider& alertTextProvider);

    void onItemClicked(const QModelIndex& index);
    void updateAlertMessage();

    bool showInvalidResources() const;
    void setShowInvalidResources(bool show);

    ResourceSelectionWidget* q;
    int columnCount;
    const ResourceValidator resourceValidator;
    const AlertTextProvider alertTextProvider;
    std::unique_ptr<InvalidResourceDecoratorModel> invalidResourceDecoratorModel;
    std::unique_ptr<InvalidResourceFilterModel> invalidResourceFilterModel;
    std::unique_ptr<ResourceSelectionDecoratorModel> selectionDecoratorModel;
    QPersistentModelIndex lastToggledIndex;
};

ResourceSelectionWidget::Private::Private(
    ResourceSelectionWidget* owner,
    int columnCount,
    const ResourceValidator& resourceValidator,
    const AlertTextProvider& alertTextProvider)
    :
    q(owner),
    columnCount(columnCount),
    resourceValidator(resourceValidator),
    alertTextProvider(alertTextProvider),
    invalidResourceDecoratorModel(new InvalidResourceDecoratorModel(resourceValidator)),
    invalidResourceFilterModel(new InvalidResourceFilterModel())
{
    selectionDecoratorModel.reset(new ResourceSelectionDecoratorModel());

    connect(q->resourceViewWidget(), &FilteredResourceViewWidget::itemClicked, this,
        &ResourceSelectionWidget::Private::onItemClicked);
}

void ResourceSelectionWidget::Private::onItemClicked(const QModelIndex& index)
{
    if (QApplication::keyboardModifiers().testFlag(Qt::AltModifier))
        return;

    auto resourceIndex = index.siblingAtColumn(resource_selection_view::ResourceColumn);

    if (resourceIndex.model() != selectionDecoratorModel.get())
    {
        const auto proxyModel = dynamic_cast<const QAbstractProxyModel*>(resourceIndex.model());
        resourceIndex = proxyModel->mapToSource(resourceIndex);
    }

    const bool hasChanges =
        QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier) && lastToggledIndex.isValid()
            ? selectionDecoratorModel->toggleSelection(lastToggledIndex, resourceIndex)
            : selectionDecoratorModel->toggleSelection(resourceIndex);

    if (!hasChanges)
        return;

    lastToggledIndex = resourceIndex;

    updateAlertMessage();

    emit q->selectionChanged();
}

void ResourceSelectionWidget::Private::updateAlertMessage()
{
    if (alertTextProvider)
    {
        const auto alertText = alertTextProvider(selectionDecoratorModel->selectedResources());
        if (!alertText.trimmed().isEmpty())
        {
            q->resourceViewWidget()->setInvalidMessage(alertText);
            return;
        }
    }
    q->resourceViewWidget()->clearInvalidMessage();
}

bool ResourceSelectionWidget::Private::showInvalidResources() const
{
    return invalidResourceFilterModel->showInvalidResources();
}

void ResourceSelectionWidget::Private::setShowInvalidResources(bool show)
{
    if (invalidResourceFilterModel->showInvalidResources() == show)
        return;

    invalidResourceFilterModel->setShowInvalidResources(show);
}

bool ResourceSelectionWidget::selectionHasInvalidResources() const
{
    if (!d->resourceValidator)
        return false;

    const auto resources = selectedResources();
    return !std::all_of(resources.cbegin(), resources.cend(), d->resourceValidator);
}

//-------------------------------------------------------------------------------------------------

ResourceSelectionWidget::ResourceSelectionWidget(
    QWidget* parent,
    int columnCount,
    ResourceValidator resourceValidator,
    AlertTextProvider alertTextProvider)
    :
    base_type(columnCount, parent),
    d(new Private(this, columnCount, resourceValidator, alertTextProvider))
{
    const auto checkboxColumn = columnCount - 1;

    resourceViewWidget()->setItemDelegate(new ResourceDialogItemDelegate(this));
    resourceViewWidget()->setItemDelegateForColumn(checkboxColumn,
        new CheckBoxColumnItemDelegate(this));

    resourceViewWidget()->setVisibleItemPredicate(
        [checkboxColumn](const QModelIndex& index)
        {
            if (const auto parentIndex = index.parent(); parentIndex.isValid())
            {
                const auto parentResource =
                    parentIndex.data(core::ResourceRole).value<QnResourcePtr>();
                if (parentResource && parentResource->hasFlags(Qn::server))
                    return true;
            }

            const auto checkStateData =
                index.siblingAtColumn(checkboxColumn).data(Qt::CheckStateRole);
            return !checkStateData.isNull()
                && checkStateData.value<Qt::CheckState>() == Qt::Checked;
        });
}

ResourceSelectionWidget::~ResourceSelectionWidget()
{
}

ResourceSelectionMode ResourceSelectionWidget::selectionMode() const
{
    return d->selectionDecoratorModel->selectionMode();
}

void ResourceSelectionWidget::setSelectionMode(ResourceSelectionMode mode)
{
    if (selectionMode() == mode)
        return;

    d->selectionDecoratorModel->setSelectionMode(mode);

    const auto checkboxColumn = d->columnCount - 1;
    if (auto itemDelegate = selectionItemDelegate(this, checkboxColumn))
    {
        switch (mode)
        {
            case ResourceSelectionMode::MultiSelection:
            case ResourceSelectionMode::SingleSelection:
                itemDelegate->setCheckMarkType(CheckBoxColumnItemDelegate::CheckBox);
                break;

            case ResourceSelectionMode::ExclusiveSelection:
                itemDelegate->setCheckMarkType(CheckBoxColumnItemDelegate::RadioButton);
                break;

            default:
                NX_ASSERT(false, "Unexpected selection mode");
                break;
        }
    }

    emit selectionModeChanged(selectionMode());
}

bool ResourceSelectionWidget::showRecordingIndicator() const
{
    if (auto itemDelegate = dialogItemDelegate(this))
        return itemDelegate->showRecordingIndicator();

    return false;
}

void ResourceSelectionWidget::setShowRecordingIndicator(bool show)
{
    if (auto itemDelegate = dialogItemDelegate(this))
        itemDelegate->setShowRecordingIndicator(show);
}

bool ResourceSelectionWidget::showInvalidResources() const
{
    return d->showInvalidResources();
}

void ResourceSelectionWidget::setShowInvalidResources(bool show)
{
    d->setShowInvalidResources(show);
}

QSet<QnResourcePtr> ResourceSelectionWidget::selectedResources() const
{
    return d->selectionDecoratorModel->selectedResources();
}

void ResourceSelectionWidget::setSelectedResources(const QSet<QnResourcePtr>& resources)
{
    if (d->selectionDecoratorModel->selectedResources() == resources)
        return;

    d->selectionDecoratorModel->setSelectedResources(resources);
    emit selectionChanged();
}

QnUuidSet ResourceSelectionWidget::selectedResourcesIds() const
{
    const auto resources = selectedResources();

    QnUuidSet result;
    for (const auto& resource: resources)
        result.insert(resource->getId());

    return result;
}

void ResourceSelectionWidget::setSelectedResourcesIds(const QnUuidSet& resourcesIds)
{
    const auto resourcesList = resourcePool()->getResourcesByIds(resourcesIds);
    setSelectedResources({std::cbegin(resourcesList), std::cend(resourcesList)});
}

QnResourcePtr ResourceSelectionWidget::selectedResource() const
{
    NX_ASSERT(selectionMode() != ResourceSelectionMode::MultiSelection,
        "Inappropriate getter for multi selection");

    const auto resources = selectedResources();
    return resources.isEmpty() ? QnResourcePtr() : *resources.cbegin();
}

void ResourceSelectionWidget::setSelectedResource(const QnResourcePtr& resource)
{
    QSet<QnResourcePtr> resources;
    if (!resource.isNull())
        resources.insert(resource);
    setSelectedResources(resources);
}

QnUuid ResourceSelectionWidget::selectedResourceId() const
{
    NX_ASSERT(selectionMode() != ResourceSelectionMode::MultiSelection,
        "Inappropriate getter for multi selection");

    const auto resourcesIds = selectedResourcesIds();
    return resourcesIds.isEmpty() ? QnUuid() : *resourcesIds.cbegin();
}

void ResourceSelectionWidget::setSelectedResourceId(const QnUuid& resourceId)
{
    QnUuidSet resourcesIds;
    if (!resourceId.isNull())
        resourcesIds.insert(resourceId);
    setSelectedResourcesIds(resourcesIds);
}

QAbstractItemModel* ResourceSelectionWidget::model() const
{
    if (!d->invalidResourceDecoratorModel->sourceModel())
        d->invalidResourceDecoratorModel->setSourceModel(base_type::model());

    if (!d->invalidResourceFilterModel->sourceModel())
        d->invalidResourceFilterModel->setSourceModel(d->invalidResourceDecoratorModel.get());

    if (!d->selectionDecoratorModel->sourceModel())
        d->selectionDecoratorModel->setSourceModel(d->invalidResourceFilterModel.get());

    return d->selectionDecoratorModel.get();
}

void ResourceSelectionWidget::setupHeader()
{
    const auto checkboxColumnWidth =
        nx::style::Metrics::kCheckIndicatorSize + resourceViewWidget()->sideIndentation().right();

    const auto header = resourceViewWidget()->treeHeaderView();
    const auto checkboxColumn = d->columnCount - 1;

    header->setStretchLastSection(false);
    header->resizeSection(checkboxColumn, checkboxColumnWidth);
    header->setSectionResizeMode(resource_selection_view::ResourceColumn,
        QHeaderView::ResizeMode::Stretch);
    header->setSectionResizeMode(checkboxColumn, QHeaderView::ResizeMode::Fixed);
}

void ResourceSelectionWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);

    if (d->invalidResourceDecoratorModel->hasInvalidResources(
        d->selectionDecoratorModel->selectedResources()))
    {
        d->setShowInvalidResources(true);
    }
    d->updateAlertMessage();
}

} // namespace nx::vms::client::desktop
