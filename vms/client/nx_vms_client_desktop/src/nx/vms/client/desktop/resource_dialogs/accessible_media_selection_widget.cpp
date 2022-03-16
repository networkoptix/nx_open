// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "accessible_media_selection_widget.h"

#include <QtWidgets/QHeaderView>

#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/indirect_access_column_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/models/indirect_access_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>

namespace nx::vms::client::desktop {

AccessibleMediaSelectionWidget::AccessibleMediaSelectionWidget(QWidget* parent):
    base_type(parent, accessible_media_selection_view::ColumnCount),
    m_indirectAccessDecoratorModel(new IndirectAccessDecoratorModel(
        resourceAccessProvider(),
        accessible_media_selection_view::IndirectResourceAccessIconColumn))
{
    resourceViewWidget()->setItemDelegateForColumn(
        accessible_media_selection_view::IndirectResourceAccessIconColumn,
        new IndirectAccessColumnItemDelegate(this));
}

QAbstractItemModel* AccessibleMediaSelectionWidget::model() const
{
    if (!m_indirectAccessDecoratorModel->sourceModel())
        m_indirectAccessDecoratorModel->setSourceModel(base_type::model());

    return m_indirectAccessDecoratorModel.get();
}

void AccessibleMediaSelectionWidget::setupHeader()
{
    using namespace accessible_media_selection_view;

    const auto checkboxColumnWidth =
        nx::style::Metrics::kCheckIndicatorSize + resourceViewWidget()->sideIndentation().right();

    static constexpr int kIndirectAccessColumnWidth =
        nx::style::Metrics::kDefaultIconSize + nx::style::Metrics::kDefaultTopLevelMargin;

    const auto header = resourceViewWidget()->treeHeaderView();

    header->setStretchLastSection(false);
    header->resizeSection(CheckboxColumn, checkboxColumnWidth);
    header->resizeSection(IndirectResourceAccessIconColumn, kIndirectAccessColumnWidth);

    header->setSectionResizeMode(ResourceColumn, QHeaderView::ResizeMode::Stretch);
    header->setSectionResizeMode(IndirectResourceAccessIconColumn, QHeaderView::ResizeMode::Fixed);
    header->setSectionResizeMode(CheckboxColumn, QHeaderView::ResizeMode::Fixed);
}

AccessibleMediaSelectionWidget::~AccessibleMediaSelectionWidget()
{
}

void AccessibleMediaSelectionWidget::setSubject(const QnResourceAccessSubject& subject)
{
    m_indirectAccessDecoratorModel->setSubject(subject);
}

void AccessibleMediaSelectionWidget::setAccessAllMedia(bool value)
{
    m_indirectAccessDecoratorModel->setAccessAllMedia(value);
}

} // namespace nx::vms::client::desktop
